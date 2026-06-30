#include "lobster/book.hpp"

namespace lobster {

namespace {

template <typename Map>
void upsert_or_erase(Map& levels, std::int64_t price_ticks, std::uint64_t qty) {
  if (qty == 0) {
    levels.erase(price_ticks);
    return;
  }
  levels[price_ticks] = qty;
}

}  // namespace

void OrderBook::apply(const BookEvent& event) {
  if (event.kind != EventKind::book_level && event.kind != EventKind::snapshot_level) {
    return;
  }
  if (event.side == Side::bid) {
    upsert_or_erase(bids_, event.price_ticks, event.qty);
    return;
  }
  upsert_or_erase(asks_, event.price_ticks, event.qty);
}

namespace {

template <typename Map>
DepthLevels depth_levels_from(const Map& levels) {
  DepthLevels depth{};
  std::uint64_t running = 0;
  std::size_t index = 0;
  for (const auto& [_, qty] : levels) {
    running += qty;
    ++index;
    if (index == 1) {
      depth.depth_1 = running;
    }
    if (index == 5) {
      depth.depth_5 = running;
    }
    if (index == 10) {
      depth.depth_10 = running;
      break;
    }
  }
  if (!depth.depth_5 && index > 0) {
    depth.depth_5 = running;
  }
  if (!depth.depth_10 && index > 0) {
    depth.depth_10 = running;
  }
  return depth;
}

}  // namespace

TopOfBook OrderBook::top_of_book() const {
  TopOfBook top{};
  if (!bids_.empty()) {
    top.best_bid_price = bids_.begin()->first;
    top.best_bid_qty = bids_.begin()->second;
  }
  if (!asks_.empty()) {
    top.best_ask_price = asks_.begin()->first;
    top.best_ask_qty = asks_.begin()->second;
  }
  return top;
}

DepthLevels OrderBook::bid_depth_levels() const { return depth_levels_from(bids_); }

DepthLevels OrderBook::ask_depth_levels() const { return depth_levels_from(asks_); }

std::size_t OrderBook::bid_levels() const { return bids_.size(); }

std::size_t OrderBook::ask_levels() const { return asks_.size(); }

}  // namespace lobster
