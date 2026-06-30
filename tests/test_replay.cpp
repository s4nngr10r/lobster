#include <cassert>
#include <filesystem>
#include <fstream>

#include "lobster/journal.hpp"
#include "lobster/replay.hpp"

int main() {
  const auto path = std::filesystem::temp_directory_path() / "lobster-test-replay.bin";

  {
    lobster::JournalWriter writer(path);
    writer.append({.ts_ns = 1,
                   .seq = 1,
                   .price_ticks = 99,
                   .qty = 2,
                   .side = lobster::Side::bid,
                   .kind = lobster::EventKind::snapshot_level});
    writer.append({.ts_ns = 1,
                   .seq = 1,
                   .price_ticks = 100,
                   .qty = 5,
                   .side = lobster::Side::ask,
                   .kind = lobster::EventKind::snapshot_level});
    writer.append({.ts_ns = 2,
                   .seq = 2,
                   .price_ticks = 101,
                   .qty = 3,
                   .side = lobster::Side::ask,
                   .kind = lobster::EventKind::book_level});
  }

  lobster::ReplayEngine replay;
  std::uint64_t seq_sum = 0;
  replay.replay(path, [&](const lobster::BookEvent& event) { seq_sum += event.seq; });
  assert(seq_sum == 4);

  {
    lobster::JournalWriter writer(path);
    writer.append({.ts_ns = 1,
                   .seq = 2,
                   .price_ticks = 100,
                   .qty = 5,
                   .side = lobster::Side::bid,
                   .kind = lobster::EventKind::book_level});
    writer.append({.ts_ns = 2,
                   .seq = 1,
                   .price_ticks = 101,
                   .qty = 3,
                   .side = lobster::Side::ask,
                   .kind = lobster::EventKind::book_level});
  }

  bool threw = false;
  try {
    replay.replay(path, [](const lobster::BookEvent&) {});
  } catch (const lobster::ReplayError&) {
    threw = true;
  }
  assert(threw);

  {
    lobster::JournalWriter writer(path);
    writer.append({.ts_ns = 1,
                   .seq = 10,
                   .price_ticks = 100,
                   .qty = 5,
                   .side = lobster::Side::bid,
                   .kind = lobster::EventKind::snapshot_level});
    writer.append({.ts_ns = 2,
                   .seq = 12,
                   .price_ticks = 101,
                   .qty = 3,
                   .side = lobster::Side::ask,
                   .kind = lobster::EventKind::book_level});
  }

  threw = false;
  try {
    replay.replay(path, [](const lobster::BookEvent&) {});
  } catch (const lobster::ReplayError&) {
    threw = true;
  }
  assert(threw);

  {
    lobster::JournalWriter writer(path);
    writer.append({.ts_ns = 1,
                   .seq = 1,
                   .price_ticks = 100,
                   .qty = 5,
                   .side = lobster::Side::bid,
                   .kind = lobster::EventKind::book_level});
  }
  std::filesystem::resize_file(path, std::filesystem::file_size(path) - 1);

  threw = false;
  try {
    replay.replay(path, [](const lobster::BookEvent&) {});
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);
  return 0;
}
