#include <cassert>
#include <string_view>
#include <vector>

#include "lobster/binance_usdtm.hpp"

int main() {
  const lobster::BinanceUsdtmConfig config{};

  const auto snapshot = lobster::parse_binance_snapshot_json(
      R"({"lastUpdateId":100,"E":1710000000000,"T":1710000000001,"bids":[["60000.1","1.500"]],"asks":[["60000.2","2.250"]]})",
      config);
  assert(snapshot.last_update_id == 100);
  assert(snapshot.event_time_ns == 1710000000000ULL * 1000000ULL);
  assert(snapshot.bids.size() == 1);
  assert(snapshot.asks.size() == 1);
  assert(snapshot.bids[0].price_ticks == 6000010000000LL);
  assert(snapshot.bids[0].qty == 150000000ULL);

  const auto buffered = lobster::parse_binance_depth_stream_json(
      R"({"stream":"btcusdt@depth@100ms","data":{"e":"depthUpdate","E":1710000000002,"T":1710000000003,"s":"BTCUSDT","U":99,"u":101,"pu":98,"b":[["60000.1","1.750"]],"a":[]}})",
      config);
  const auto live = lobster::parse_binance_depth_stream_json(
      R"({"stream":"btcusdt@depth@100ms","data":{"e":"depthUpdate","E":1710000000004,"T":1710000000005,"s":"BTCUSDT","U":102,"u":103,"pu":101,"b":[],"a":[["60000.2","0"]]}})",
      config);
  const auto trade = lobster::parse_binance_trade_stream_json(
      R"({"stream":"btcusdt@aggTrade","data":{"e":"aggTrade","E":1710000000006,"T":1710000000007,"a":77,"p":"60000.3","q":"0.250","m":true}})",
      config);
  assert(trade.trade_id == 77);
  assert(trade.side == lobster::Side::ask);
  assert(trade.qty == 25000000ULL);

  lobster::BinanceBookSynchronizer synchronizer;
  synchronizer.buffer_update(buffered);

  std::vector<lobster::BookEvent> emitted;
  const auto emit = [&](const lobster::BookEvent& event) { emitted.push_back(event); };
  synchronizer.apply_snapshot(snapshot, emit);
  synchronizer.on_live_update(live, emit);

  assert(emitted.size() == 4);
  assert(emitted[0].kind == lobster::EventKind::snapshot_level);
  assert(emitted[1].kind == lobster::EventKind::snapshot_level);
  assert(emitted[2].kind == lobster::EventKind::book_level);
  assert(emitted[2].seq == 3);
  assert(emitted[2].source_first_id == 99);
  assert(emitted[2].source_last_id == 101);
  assert(emitted[2].side == lobster::Side::bid);
  assert(emitted[2].qty == 175000000ULL);
  assert(emitted[3].seq == 4);
  assert(emitted[3].side == lobster::Side::ask);
  assert(emitted[3].qty == 0);
  const auto heartbeat = synchronizer.make_heartbeat_event(live);
  assert(heartbeat.kind == lobster::EventKind::heartbeat);
  assert(heartbeat.source_first_id == 102);
  const auto trade_event = synchronizer.make_trade_event(trade);
  assert(trade_event.kind == lobster::EventKind::trade);
  assert(trade_event.price_ticks == 6000030000000LL);

  lobster::BinanceBookSynchronizer broken;
  broken.buffer_update(buffered);
  broken.apply_snapshot(snapshot, [](const lobster::BookEvent&) {});
  bool threw = false;
  try {
    broken.on_live_update(
        lobster::parse_binance_depth_stream_json(
            R"({"stream":"btcusdt@depth@100ms","data":{"e":"depthUpdate","E":1710000000006,"T":1710000000007,"s":"BTCUSDT","U":104,"u":105,"pu":999,"b":[],"a":[]}})",
            config),
        [](const lobster::BookEvent&) {});
  } catch (const lobster::BinanceError&) {
    threw = true;
  }
  assert(threw);
  return 0;
}
