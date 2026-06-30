#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sys/resource.h>

#include "lobster/book.hpp"
#include "lobster/journal.hpp"
#include "lobster/replay.hpp"

int main(int argc, char** argv) {
  const std::uint64_t event_count =
      argc == 2 ? std::strtoull(argv[1], nullptr, 10) : 1000000ULL;
  const auto path = std::filesystem::temp_directory_path() / "lobster-bench-memory.bin";

  {
    lobster::JournalWriter writer(path);
    for (std::uint64_t i = 1; i <= event_count; ++i) {
      writer.append(lobster::BookEvent{
          .ts_ns = i,
          .seq = i,
          .source_first_id = i,
          .source_last_id = i,
          .price_ticks = 10000 + static_cast<std::int64_t>(i % 1024),
          .qty = 1 + (i % 16),
          .side = (i % 2 == 0) ? lobster::Side::bid : lobster::Side::ask,
          .kind = lobster::EventKind::book_level,
      });
    }
  }

  lobster::OrderBook book;
  lobster::ReplayEngine replay;
  replay.replay(path, [&](const lobster::BookEvent& event) { book.apply(event); });

  rusage usage{};
  getrusage(RUSAGE_SELF, &usage);
  std::cout << "events=" << event_count << " maxrss_kb=" << usage.ru_maxrss
            << " bid_levels=" << book.bid_levels() << " ask_levels=" << book.ask_levels()
            << '\n';
  return EXIT_SUCCESS;
}
