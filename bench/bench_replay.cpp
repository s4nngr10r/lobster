#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>

#include "lobster/book.hpp"
#include "lobster/journal.hpp"
#include "lobster/replay.hpp"

int main(int argc, char** argv) {
  const std::uint64_t event_count =
      argc == 2 ? std::strtoull(argv[1], nullptr, 10) : 1000000ULL;
  const auto path = std::filesystem::temp_directory_path() / "lobster-bench.bin";

  {
    lobster::JournalWriter writer(path);
    for (std::uint64_t i = 1; i <= event_count; ++i) {
      writer.append(lobster::BookEvent{
          .ts_ns = i,
          .seq = i,
          .price_ticks = 10000 + static_cast<std::int64_t>(i % 64),
          .qty = 1 + (i % 8),
          .side = (i % 2 == 0) ? lobster::Side::bid : lobster::Side::ask,
          .kind = lobster::EventKind::book_level,
      });
    }
  }

  lobster::OrderBook book;
  lobster::ReplayEngine replay;
  const auto start = std::chrono::steady_clock::now();
  replay.replay(path, [&](const lobster::BookEvent& event) { book.apply(event); });
  const auto stop = std::chrono::steady_clock::now();

  const auto elapsed_ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
  const double per_event_ns = static_cast<double>(elapsed_ns) / event_count;
  const double events_per_second =
      1'000'000'000.0 * static_cast<double>(event_count) / elapsed_ns;

  std::cout << "events=" << event_count << " elapsed_ns=" << elapsed_ns
            << " per_event_ns=" << per_event_ns
            << " events_per_second=" << events_per_second << '\n';
  return EXIT_SUCCESS;
}
