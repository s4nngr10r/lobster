#include "lobster/binance_usdtm.hpp"

#include <atomic>
#include <csignal>
#include <cstddef>
#include <curl/curl.h>
#include <libwebsockets.h>
#include <mutex>
#include <optional>
#include <queue>
#include <simdjson.h>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "lobster/journal.hpp"

namespace lobster {

namespace {

std::atomic<bool> g_stop_requested = false;

void signal_handler(int) { g_stop_requested.store(true); }

std::uint64_t parse_scaled_unsigned(std::string_view value, std::uint64_t scale) {
  if (scale == 0) {
    throw BinanceError("scale must be non-zero");
  }

  std::uint64_t integer = 0;
  std::uint64_t fractional = 0;
  std::uint64_t fractional_scale = scale;
  bool seen_dot = false;

  for (const char ch : value) {
    if (ch == '.') {
      if (seen_dot) {
        throw BinanceError("invalid decimal value");
      }
      seen_dot = true;
      continue;
    }
    if (ch < '0' || ch > '9') {
      throw BinanceError("invalid decimal value");
    }
    const auto digit = static_cast<std::uint64_t>(ch - '0');
    if (!seen_dot) {
      integer = integer * 10 + digit;
      continue;
    }
    if (fractional_scale > 1) {
      fractional = fractional * 10 + digit;
      fractional_scale /= 10;
      continue;
    }
    if (digit != 0) {
      throw BinanceError("value exceeds configured precision");
    }
  }

  while (fractional_scale > 1) {
    fractional *= 10;
    fractional_scale /= 10;
  }

  return integer * scale + fractional;
}

std::vector<BinancePriceLevel> parse_levels(simdjson::ondemand::array levels,
                                            std::uint64_t price_scale,
                                            std::uint64_t qty_scale) {
  std::vector<BinancePriceLevel> parsed;
  for (auto level_node : levels) {
    auto pair = level_node.get_array();
    auto it = pair.begin();
    if (it == pair.end()) {
      throw BinanceError("missing price in depth level");
    }
    const auto price = std::string_view(*it);
    ++it;
    if (it == pair.end()) {
      throw BinanceError("missing quantity in depth level");
    }
    const auto qty = std::string_view(*it);
    parsed.push_back(BinancePriceLevel{
        .price_ticks = parse_decimal_ticks(price, price_scale),
        .qty = parse_decimal_qty(qty, qty_scale),
    });
  }
  return parsed;
}

template <typename T>
T parse_json(std::string_view json, const std::function<T(simdjson::ondemand::document&)>& fn) {
  simdjson::ondemand::parser parser;
  std::string owned(json);
  auto doc = parser.iterate(owned);
  if (doc.error()) {
    throw BinanceError("failed to parse JSON payload");
  }
  return fn(doc.value_unsafe());
}

std::string event_type_from_message(std::string_view json) {
  return parse_json<std::string>(json, [](simdjson::ondemand::document& doc) {
    auto root = doc.get_object();
    auto data = root["data"];
    auto object = data.error() == simdjson::SUCCESS ? data.get_object() : root;
    return std::string(std::string_view(object["e"]));
  });
}

std::size_t write_callback(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
  auto* output = static_cast<std::string*>(userdata);
  output->append(ptr, size * nmemb);
  return size * nmemb;
}

class BinanceWsStream {
 public:
  BinanceWsStream() = default;

  void start() {
    worker_ = std::thread([this] { run(); });
  }

  void stop() {
    stop_requested_.store(true);
    if (context_ != nullptr) {
      lws_cancel_service(context_);
    }
    if (worker_.joinable()) {
      worker_.join();
    }
  }

  void wait_until_connected() {
    for (;;) {
      {
        std::scoped_lock lock(mu_);
        if (!error_.empty()) {
          throw BinanceError(error_);
        }
        if (connected_) {
          return;
        }
      }
      if (stop_requested_.load()) {
        throw BinanceError("websocket stopped before connect");
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  bool try_pop(std::string& message) {
    std::scoped_lock lock(mu_);
    if (messages_.empty()) {
      return false;
    }
    message = std::move(messages_.front());
    messages_.pop();
    return true;
  }

  bool wait_and_pop(std::string& message) {
    for (;;) {
      {
        std::scoped_lock lock(mu_);
        if (!messages_.empty()) {
          message = std::move(messages_.front());
          messages_.pop();
          return true;
        }
        if (!error_.empty()) {
          throw BinanceError(error_);
        }
      }
      if (stop_requested_.load()) {
        return false;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

 private:
  struct SessionData {
    std::string partial;
  };

  static int callback(lws* wsi, lws_callback_reasons reason, void* user, void* in,
                      std::size_t len) {
    auto* self = static_cast<BinanceWsStream*>(lws_context_user(lws_get_context(wsi)));
    auto* session = static_cast<SessionData*>(user);
    switch (reason) {
      case LWS_CALLBACK_CLIENT_ESTABLISHED:
        self->mark_connected();
        return 0;
      case LWS_CALLBACK_CLIENT_RECEIVE:
        session->partial.append(static_cast<const char*>(in), len);
        if (lws_is_final_fragment(wsi) && lws_remaining_packet_payload(wsi) == 0) {
          self->push_message(std::move(session->partial));
          session->partial.clear();
        }
        return 0;
      case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        self->set_error(in == nullptr ? "websocket connection error"
                                      : std::string(static_cast<const char*>(in), len));
        return 0;
      case LWS_CALLBACK_CLIENT_CLOSED:
        self->stop_requested_.store(true);
        return 0;
      default:
        return 0;
    }
  }

  void mark_connected() {
    std::scoped_lock lock(mu_);
    connected_ = true;
  }

  void push_message(std::string&& message) {
    std::scoped_lock lock(mu_);
    messages_.push(std::move(message));
  }

  void set_error(std::string message) {
    std::scoped_lock lock(mu_);
    error_ = std::move(message);
    stop_requested_.store(true);
  }

  void run() {
    static lws_protocols protocols[] = {
        {"lobster-binance-depth", &BinanceWsStream::callback, sizeof(SessionData), 0, 0,
         nullptr, 0},
        LWS_PROTOCOL_LIST_TERM,
    };

    lws_context_creation_info info{};
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.user = this;

    context_ = lws_create_context(&info);
    if (context_ == nullptr) {
      set_error("failed to create websocket context");
      return;
    }

    lws_client_connect_info connect_info{};
    connect_info.context = context_;
    connect_info.address = "fstream.binance.com";
    connect_info.port = 443;
    connect_info.path =
        "/public/stream?streams=btcusdt@depth@100ms/btcusdt@aggTrade";
    connect_info.host = connect_info.address;
    connect_info.origin = connect_info.address;
    connect_info.protocol = protocols[0].name;
    connect_info.ssl_connection = LCCSCF_USE_SSL;

    if (lws_client_connect_via_info(&connect_info) == nullptr) {
      set_error("failed to create websocket connection");
      lws_context_destroy(context_);
      context_ = nullptr;
      return;
    }

    while (!stop_requested_.load() && !g_stop_requested.load()) {
      lws_service(context_, 50);
    }

    lws_context_destroy(context_);
    context_ = nullptr;
  }

  std::mutex mu_{};
  std::queue<std::string> messages_{};
  std::thread worker_{};
  lws_context* context_{nullptr};
  std::atomic<bool> stop_requested_{false};
  bool connected_{false};
  std::string error_{};
};

}  // namespace

std::int64_t parse_decimal_ticks(std::string_view value, std::uint64_t scale) {
  return static_cast<std::int64_t>(parse_scaled_unsigned(value, scale));
}

std::uint64_t parse_decimal_qty(std::string_view value, std::uint64_t scale) {
  return parse_scaled_unsigned(value, scale);
}

BinanceDepthSnapshot parse_binance_snapshot_json(std::string_view json,
                                                 const BinanceUsdtmConfig& config) {
  return parse_json<BinanceDepthSnapshot>(
      json, [&](simdjson::ondemand::document& doc) {
        auto object = doc.get_object();
        const auto last_update_id = static_cast<std::uint64_t>(object["lastUpdateId"]);
        const auto event_time_ms = static_cast<std::uint64_t>(object["E"]);
        return BinanceDepthSnapshot{
            .last_update_id = last_update_id,
            .event_time_ns = event_time_ms * 1000000ULL,
            .bids = parse_levels(object["bids"].get_array(), config.price_scale,
                                 config.qty_scale),
            .asks = parse_levels(object["asks"].get_array(), config.price_scale,
                                 config.qty_scale),
        };
      });
}

BinanceDepthUpdate parse_binance_depth_stream_json(std::string_view json,
                                                   const BinanceUsdtmConfig& config) {
  return parse_json<BinanceDepthUpdate>(
      json, [&](simdjson::ondemand::document& doc) {
        auto root = doc.get_object();
        auto data = root["data"];
        auto object =
            data.error() == simdjson::SUCCESS ? data.get_object() : root;
        return BinanceDepthUpdate{
            .event_time_ns = static_cast<std::uint64_t>(object["E"]) * 1000000ULL,
            .transaction_time_ns = static_cast<std::uint64_t>(object["T"]) * 1000000ULL,
            .first_update_id = static_cast<std::uint64_t>(object["U"]),
            .final_update_id = static_cast<std::uint64_t>(object["u"]),
            .previous_final_update_id = static_cast<std::uint64_t>(object["pu"]),
            .bids = parse_levels(object["b"].get_array(), config.price_scale,
                                 config.qty_scale),
            .asks = parse_levels(object["a"].get_array(), config.price_scale,
                                 config.qty_scale),
        };
      });
}

BinanceTrade parse_binance_trade_stream_json(std::string_view json,
                                             const BinanceUsdtmConfig& config) {
  return parse_json<BinanceTrade>(
      json, [&](simdjson::ondemand::document& doc) {
        auto root = doc.get_object();
        auto data = root["data"];
        auto object = data.error() == simdjson::SUCCESS ? data.get_object() : root;
        const bool buyer_maker = static_cast<bool>(object["m"]);
        return BinanceTrade{
            .event_time_ns = static_cast<std::uint64_t>(object["E"]) * 1000000ULL,
            .trade_time_ns = static_cast<std::uint64_t>(object["T"]) * 1000000ULL,
            .trade_id = static_cast<std::uint64_t>(object["a"]),
            .price_ticks =
                parse_decimal_ticks(std::string_view(object["p"]), config.price_scale),
            .qty = parse_decimal_qty(std::string_view(object["q"]), config.qty_scale),
            .side = buyer_maker ? Side::ask : Side::bid,
        };
      });
}

bool binance_message_is_trade(std::string_view json) {
  return event_type_from_message(json) == "aggTrade";
}

bool binance_message_is_depth(std::string_view json) {
  return event_type_from_message(json) == "depthUpdate";
}

void BinanceBookSynchronizer::buffer_update(const BinanceDepthUpdate& update) {
  if (!buffered_updates_.empty() &&
      update.final_update_id <= buffered_updates_.back().final_update_id) {
    throw BinanceError("binance updates must be buffered in ascending order");
  }
  buffered_updates_.push_back(update);
}

void BinanceBookSynchronizer::reset() {
  buffered_updates_.clear();
  snapshot_update_id_.reset();
  previous_stream_final_update_id_.reset();
}

void BinanceBookSynchronizer::apply_snapshot(const BinanceDepthSnapshot& snapshot,
                                             const Handler& emit) {
  if (snapshot_update_id_.has_value()) {
    throw BinanceError("snapshot already applied");
  }

  snapshot_update_id_ = snapshot.last_update_id;
  for (const auto& level : snapshot.bids) {
    emit(make_event(snapshot.event_time_ns, Side::bid, EventKind::snapshot_level, level,
                    snapshot.last_update_id, snapshot.last_update_id));
  }
  for (const auto& level : snapshot.asks) {
    emit(make_event(snapshot.event_time_ns, Side::ask, EventKind::snapshot_level, level,
                    snapshot.last_update_id, snapshot.last_update_id));
  }

  std::vector<BinanceDepthUpdate> retained;
  retained.reserve(buffered_updates_.size());
  for (const auto& update : buffered_updates_) {
    if (update.final_update_id >= snapshot.last_update_id) {
      retained.push_back(update);
    }
  }
  buffered_updates_ = std::move(retained);

  std::vector<BinanceDepthUpdate> pending;
  pending.swap(buffered_updates_);
  for (const auto& update : pending) {
    process_after_snapshot(update, emit);
  }
}

void BinanceBookSynchronizer::on_live_update(const BinanceDepthUpdate& update,
                                             const Handler& emit) {
  if (!snapshot_update_id_.has_value()) {
    buffer_update(update);
    return;
  }
  process_after_snapshot(update, emit);
}

void BinanceBookSynchronizer::process_after_snapshot(const BinanceDepthUpdate& update,
                                                     const Handler& emit) {
  if (!snapshot_update_id_.has_value()) {
    throw BinanceError("snapshot must be applied before processing live updates");
  }

  if (!previous_stream_final_update_id_.has_value()) {
    if (update.final_update_id < *snapshot_update_id_) {
      return;
    }
    if (update.first_update_id > *snapshot_update_id_ ||
        update.final_update_id < *snapshot_update_id_) {
      throw BinanceError("no diff event bridged the Binance snapshot update id");
    }
    process_update(update, emit);
    previous_stream_final_update_id_ = update.final_update_id;
    return;
  }

  if (update.previous_final_update_id != *previous_stream_final_update_id_) {
    throw BinanceError("Binance diff stream gap detected via pu/u mismatch");
  }

  process_update(update, emit);
  previous_stream_final_update_id_ = update.final_update_id;
}

void BinanceBookSynchronizer::process_update(const BinanceDepthUpdate& update,
                                             const Handler& emit) {
  for (const auto& level : update.bids) {
    emit(make_event(update.transaction_time_ns, Side::bid, EventKind::book_level, level,
                    update.first_update_id, update.final_update_id));
  }
  for (const auto& level : update.asks) {
    emit(make_event(update.transaction_time_ns, Side::ask, EventKind::book_level, level,
                    update.first_update_id, update.final_update_id));
  }
}

BookEvent BinanceBookSynchronizer::make_event(std::uint64_t ts_ns, Side side, EventKind kind,
                                              const BinancePriceLevel& level,
                                              std::uint64_t source_first_id,
                                              std::uint64_t source_last_id) {
  return BookEvent{
      .ts_ns = ts_ns,
      .seq = next_local_seq_++,
      .source_first_id = source_first_id,
      .source_last_id = source_last_id,
      .price_ticks = level.price_ticks,
      .qty = level.qty,
      .side = side,
      .kind = kind,
  };
}

BookEvent BinanceBookSynchronizer::make_trade_event(const BinanceTrade& trade) {
  return BookEvent{
      .ts_ns = trade.trade_time_ns,
      .seq = next_local_seq_++,
      .source_first_id = trade.trade_id,
      .source_last_id = trade.trade_id,
      .price_ticks = trade.price_ticks,
      .qty = trade.qty,
      .side = trade.side,
      .kind = EventKind::trade,
  };
}

BookEvent BinanceBookSynchronizer::make_heartbeat_event(const BinanceDepthUpdate& update) {
  return BookEvent{
      .ts_ns = update.event_time_ns,
      .seq = next_local_seq_++,
      .source_first_id = update.first_update_id,
      .source_last_id = update.final_update_id,
      .price_ticks = 0,
      .qty = 0,
      .side = Side::bid,
      .kind = EventKind::heartbeat,
  };
}

BookEvent BinanceBookSynchronizer::make_venue_status_event(std::uint64_t ts_ns,
                                                           std::uint64_t source_last_id) {
  return BookEvent{
      .ts_ns = ts_ns,
      .seq = next_local_seq_++,
      .source_first_id = source_last_id,
      .source_last_id = source_last_id,
      .price_ticks = 0,
      .qty = 0,
      .side = Side::bid,
      .kind = EventKind::venue_status,
  };
}

std::string fetch_binance_snapshot(const BinanceUsdtmConfig& config) {
  curl_global_init(CURL_GLOBAL_DEFAULT);
  CURL* curl = curl_easy_init();
  if (curl == nullptr) {
    throw BinanceError("failed to initialize curl");
  }

  std::string response;
  std::ostringstream url;
  url << "https://fapi.binance.com/fapi/v1/depth?symbol=" << config.symbol
      << "&limit=" << config.depth_limit;

  curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "lobster/0.1");
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

  const auto rc = curl_easy_perform(curl);
  if (rc != CURLE_OK) {
    const std::string error = curl_easy_strerror(rc);
    curl_easy_cleanup(curl);
    throw BinanceError("failed to fetch Binance snapshot: " + error);
  }

  long status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  curl_easy_cleanup(curl);

  if (status != 200) {
    throw BinanceError("Binance snapshot HTTP status " + std::to_string(status));
  }
  return response;
}

void capture_binance_usdtm_btcusdt(const std::string& journal_path,
                                   const BinanceUsdtmConfig& config) {
  std::signal(SIGINT, signal_handler);
  g_stop_requested.store(false);

  JournalWriter writer(journal_path);
  BinanceBookSynchronizer synchronizer;
  BinanceWsStream ws;
  ws.start();
  ws.wait_until_connected();

  try {
    auto bootstrap = [&] {
      synchronizer.reset();
      const auto snapshot_json = fetch_binance_snapshot(config);
      const auto snapshot = parse_binance_snapshot_json(snapshot_json, config);
      writer.append(
          synchronizer.make_venue_status_event(snapshot.event_time_ns, snapshot.last_update_id));

      std::string buffered;
      while (ws.try_pop(buffered)) {
        if (binance_message_is_depth(buffered)) {
          synchronizer.buffer_update(parse_binance_depth_stream_json(buffered, config));
          continue;
        }
        if (binance_message_is_trade(buffered)) {
          writer.append(
              synchronizer.make_trade_event(parse_binance_trade_stream_json(buffered, config)));
        }
      }

      synchronizer.apply_snapshot(snapshot,
                                  [&](const BookEvent& event) { writer.append(event); });
    };
    bootstrap();

    std::string message;
    while (!g_stop_requested.load()) {
      if (!ws.wait_and_pop(message)) {
        break;
      }
      if (binance_message_is_trade(message)) {
        writer.append(
            synchronizer.make_trade_event(parse_binance_trade_stream_json(message, config)));
        continue;
      }
      if (!binance_message_is_depth(message)) {
        continue;
      }
      const auto update = parse_binance_depth_stream_json(message, config);
      writer.append(synchronizer.make_heartbeat_event(update));
      try {
        synchronizer.on_live_update(update,
                                    [&](const BookEvent& event) { writer.append(event); });
      } catch (const BinanceError&) {
        bootstrap();
      }
    }
  } catch (...) {
    ws.stop();
    throw;
  }

  ws.stop();
}

}  // namespace lobster
