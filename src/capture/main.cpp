#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "lobster/binance_usdtm.hpp"
#include "lobster/bootstrap.hpp"
#include "lobster/journal.hpp"

namespace {

lobster::Side parse_side(const std::string& token) {
  if (token == "B") {
    return lobster::Side::bid;
  }
  if (token == "A") {
    return lobster::Side::ask;
  }
  throw std::runtime_error("side must be B or A");
}

lobster::BookEvent parse_event_line(const std::string& line, const std::string& op) {
  std::istringstream input(line);
  std::string parsed_op;
  lobster::BookEvent event{};
  std::string side_token;
  input >> parsed_op >> event.ts_ns >> event.seq >> side_token >> event.price_ticks >>
      event.qty;
  if (!input || parsed_op != op) {
    throw std::runtime_error("failed to parse capture event");
  }
  event.side = parse_side(side_token);
  return event;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2 && argc != 3) {
    std::cerr << "usage: lobster-capture <journal-path>\n"
                 "   or: lobster-capture --binance-usdtm-btcusdt <journal-path>\n";
    return EXIT_FAILURE;
  }

  try {
    if (argc == 3) {
      const std::string mode = argv[1];
      if (mode != "--binance-usdtm-btcusdt") {
        throw std::runtime_error("unknown capture mode");
      }
      lobster::capture_binance_usdtm_btcusdt(argv[2], lobster::BinanceUsdtmConfig{});
      return EXIT_SUCCESS;
    }

    lobster::JournalWriter writer(argv[1]);
    lobster::SnapshotBootstrap bootstrap;
    std::string line;
    while (std::getline(std::cin, line)) {
      if (line.empty()) {
        continue;
      }
      if (line.rfind("SNAP ", 0) == 0) {
        bootstrap.on_snapshot(parse_event_line(line, "SNAP"));
        continue;
      }
      if (line.rfind("UPD ", 0) == 0) {
        bootstrap.on_incremental(parse_event_line(line, "UPD"),
                                 [&](const lobster::BookEvent& event) {
                                   writer.append(event);
                                 });
        continue;
      }
      if (line.rfind("SYNC ", 0) == 0) {
        std::istringstream input(line);
        std::string op;
        std::uint64_t snapshot_seq = 0;
        input >> op >> snapshot_seq;
        if (!input || op != "SYNC") {
          throw std::runtime_error("failed to parse sync line");
        }
        bootstrap.sync_to(snapshot_seq, [&](const lobster::BookEvent& event) {
          writer.append(event);
        });
        continue;
      }
      throw std::runtime_error("unknown capture line");
    }
    if (!bootstrap.synced()) {
      throw std::runtime_error("missing SYNC line");
    }
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
