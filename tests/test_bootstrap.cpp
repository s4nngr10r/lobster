#include <cassert>
#include <vector>

#include "lobster/bootstrap.hpp"

int main() {
  lobster::SnapshotBootstrap bootstrap;
  std::vector<lobster::BookEvent> emitted;
  const auto emit = [&](const lobster::BookEvent& event) { emitted.push_back(event); };

  bootstrap.on_snapshot({.ts_ns = 100,
                         .seq = 10,
                         .price_ticks = 10000,
                         .qty = 5,
                         .side = lobster::Side::bid,
                         .kind = lobster::EventKind::book_level});
  bootstrap.on_snapshot({.ts_ns = 100,
                         .seq = 10,
                         .price_ticks = 10020,
                         .qty = 4,
                         .side = lobster::Side::ask,
                         .kind = lobster::EventKind::book_level});
  bootstrap.on_incremental({.ts_ns = 90,
                            .seq = 9,
                            .price_ticks = 9990,
                            .qty = 2,
                            .side = lobster::Side::bid,
                            .kind = lobster::EventKind::book_level},
                           emit);
  bootstrap.on_incremental({.ts_ns = 110,
                            .seq = 11,
                            .price_ticks = 10000,
                            .qty = 7,
                            .side = lobster::Side::bid,
                            .kind = lobster::EventKind::book_level},
                           emit);
  bootstrap.on_incremental({.ts_ns = 120,
                            .seq = 12,
                            .price_ticks = 10020,
                            .qty = 0,
                            .side = lobster::Side::ask,
                            .kind = lobster::EventKind::book_level},
                           emit);

  bootstrap.sync_to(10, emit);
  assert(bootstrap.synced());
  assert(emitted.size() == 4);
  assert(emitted[0].kind == lobster::EventKind::snapshot_level);
  assert(emitted[1].kind == lobster::EventKind::snapshot_level);
  assert(emitted[2].seq == 11);
  assert(emitted[3].seq == 12);

  lobster::SnapshotBootstrap gappy;
  gappy.on_snapshot({.ts_ns = 100,
                     .seq = 20,
                     .price_ticks = 10000,
                     .qty = 5,
                     .side = lobster::Side::bid,
                     .kind = lobster::EventKind::book_level});
  gappy.on_incremental({.ts_ns = 121,
                        .seq = 22,
                        .price_ticks = 10000,
                        .qty = 7,
                        .side = lobster::Side::bid,
                        .kind = lobster::EventKind::book_level},
                       emit);

  bool threw = false;
  try {
    gappy.sync_to(20, [](const lobster::BookEvent&) {});
  } catch (const lobster::BootstrapError&) {
    threw = true;
  }
  assert(threw);
  return 0;
}
