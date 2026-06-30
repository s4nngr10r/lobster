#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "lobster/types.hpp"

namespace lobster {

struct BinanceUsdtmConfig {
  std::string symbol{"BTCUSDT"};
  std::string stream_name{"btcusdt@depth@100ms"};
  std::uint64_t price_scale{100000000};
  std::uint64_t qty_scale{100000000};
  std::uint32_t depth_limit{1000};
};

struct BinancePriceLevel {
  std::int64_t price_ticks{};
  std::uint64_t qty{};
};

struct BinanceDepthSnapshot {
  std::uint64_t last_update_id{};
  std::uint64_t event_time_ns{};
  std::vector<BinancePriceLevel> bids{};
  std::vector<BinancePriceLevel> asks{};
};

struct BinanceDepthUpdate {
  std::uint64_t event_time_ns{};
  std::uint64_t transaction_time_ns{};
  std::uint64_t first_update_id{};
  std::uint64_t final_update_id{};
  std::uint64_t previous_final_update_id{};
  std::vector<BinancePriceLevel> bids{};
  std::vector<BinancePriceLevel> asks{};
};

struct BinanceTrade {
  std::uint64_t event_time_ns{};
  std::uint64_t trade_time_ns{};
  std::uint64_t trade_id{};
  std::int64_t price_ticks{};
  std::uint64_t qty{};
  Side side{Side::bid};
};

class BinanceError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

BinanceDepthSnapshot parse_binance_snapshot_json(std::string_view json,
                                                 const BinanceUsdtmConfig& config);
BinanceDepthUpdate parse_binance_depth_stream_json(std::string_view json,
                                                   const BinanceUsdtmConfig& config);
BinanceTrade parse_binance_trade_stream_json(std::string_view json,
                                             const BinanceUsdtmConfig& config);
std::int64_t parse_decimal_ticks(std::string_view value, std::uint64_t scale);
std::uint64_t parse_decimal_qty(std::string_view value, std::uint64_t scale);

class BinanceBookSynchronizer {
 public:
  using Handler = std::function<void(const BookEvent&)>;

  void buffer_update(const BinanceDepthUpdate& update);
  void apply_snapshot(const BinanceDepthSnapshot& snapshot, const Handler& emit);
  void on_live_update(const BinanceDepthUpdate& update, const Handler& emit);
  void reset();
  BookEvent make_trade_event(const BinanceTrade& trade);
  BookEvent make_heartbeat_event(const BinanceDepthUpdate& update);
  BookEvent make_venue_status_event(std::uint64_t ts_ns, std::uint64_t source_last_id);

 private:
  void process_update(const BinanceDepthUpdate& update, const Handler& emit);
  void process_after_snapshot(const BinanceDepthUpdate& update, const Handler& emit);
  BookEvent make_event(std::uint64_t ts_ns, Side side, EventKind kind,
                       const BinancePriceLevel& level, std::uint64_t source_first_id,
                       std::uint64_t source_last_id);

  std::vector<BinanceDepthUpdate> buffered_updates_{};
  std::optional<std::uint64_t> snapshot_update_id_{};
  std::optional<std::uint64_t> previous_stream_final_update_id_{};
  std::uint64_t next_local_seq_{1};
};

std::string fetch_binance_snapshot(const BinanceUsdtmConfig& config);
bool binance_message_is_trade(std::string_view json);
bool binance_message_is_depth(std::string_view json);
void capture_binance_usdtm_btcusdt(const std::string& journal_path,
                                   const BinanceUsdtmConfig& config);

}  // namespace lobster
