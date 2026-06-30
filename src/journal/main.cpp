#include <cstdlib>
#include <iostream>
#include <string_view>

#include "lobster/journal.hpp"

namespace {

std::string_view side_name(lobster::Side side) {
  return side == lobster::Side::bid ? "bid" : "ask";
}

std::string_view kind_name(lobster::EventKind kind) {
  switch (kind) {
    case lobster::EventKind::snapshot_level:
      return "snapshot";
    case lobster::EventKind::book_level:
      return "live";
    case lobster::EventKind::trade:
      return "trade";
    case lobster::EventKind::heartbeat:
      return "heartbeat";
    case lobster::EventKind::venue_status:
      return "venue_status";
  }
  return "unknown";
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: lobster-dump <journal-path>\n";
    return EXIT_FAILURE;
  }

  try {
    lobster::JournalReader reader(argv[1]);
    std::cout << "seq,ts_ns,source_first_id,source_last_id,kind,side,price_ticks,qty\n";
    while (const auto event = reader.next()) {
      std::cout << event->seq << ',' << event->ts_ns << ',' << event->source_first_id
                << ',' << event->source_last_id << ',' << kind_name(event->kind) << ','
                << side_name(event->side) << ',' << event->price_ticks << ','
                << event->qty << '\n';
    }
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
