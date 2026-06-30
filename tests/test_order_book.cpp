#include <cassert>

#include "lobster/book.hpp"

int main() {
  lobster::OrderBook book;
  book.apply({.ts_ns = 1,
              .seq = 1,
              .price_ticks = 100,
              .qty = 10,
              .side = lobster::Side::bid,
              .kind = lobster::EventKind::snapshot_level});
  book.apply({.ts_ns = 2,
              .seq = 2,
              .price_ticks = 99,
              .qty = 8,
              .side = lobster::Side::bid,
              .kind = lobster::EventKind::book_level});
  book.apply({.ts_ns = 3,
              .seq = 3,
              .price_ticks = 105,
              .qty = 7,
              .side = lobster::Side::ask,
              .kind = lobster::EventKind::book_level});
  book.apply({.ts_ns = 4,
              .seq = 4,
              .price_ticks = 105,
              .qty = 0,
              .side = lobster::Side::ask,
              .kind = lobster::EventKind::book_level});

  const auto top = book.top_of_book();
  assert(top.best_bid_price && *top.best_bid_price == 100);
  assert(top.best_bid_qty && *top.best_bid_qty == 10);
  assert(!top.best_ask_price);
  assert(book.bid_levels() == 2);
  assert(book.ask_levels() == 0);
  return 0;
}
