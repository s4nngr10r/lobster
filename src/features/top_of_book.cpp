#include "lobster/features.hpp"

namespace lobster {

TopOfBookFeatures compute_top_of_book_features(const BookEvent& event, const TopOfBook& top,
                                               const DepthLevels& bid_depths,
                                               const DepthLevels& ask_depths,
                                               std::size_t bid_levels,
                                               std::size_t ask_levels) {
  TopOfBookFeatures row{
      .ts_ns = event.ts_ns,
      .seq = event.seq,
      .source_first_id = event.source_first_id,
      .source_last_id = event.source_last_id,
      .best_bid_price = top.best_bid_price,
      .best_bid_qty = top.best_bid_qty,
      .best_ask_price = top.best_ask_price,
      .best_ask_qty = top.best_ask_qty,
      .bid_depth_1 = bid_depths.depth_1,
      .ask_depth_1 = ask_depths.depth_1,
      .bid_depth_5 = bid_depths.depth_5,
      .ask_depth_5 = ask_depths.depth_5,
      .bid_depth_10 = bid_depths.depth_10,
      .ask_depth_10 = ask_depths.depth_10,
      .bid_levels = static_cast<std::uint64_t>(bid_levels),
      .ask_levels = static_cast<std::uint64_t>(ask_levels),
  };

  if (top.best_bid_price && top.best_ask_price) {
    row.spread_ticks = *top.best_ask_price - *top.best_bid_price;
    row.mid_price = static_cast<double>(*top.best_bid_price + *top.best_ask_price) / 2.0;
  }

  if (top.best_bid_price && top.best_ask_price && top.best_bid_qty && top.best_ask_qty) {
    const double bid_px = static_cast<double>(*top.best_bid_price);
    const double ask_px = static_cast<double>(*top.best_ask_price);
    const double bid_qty = static_cast<double>(*top.best_bid_qty);
    const double ask_qty = static_cast<double>(*top.best_ask_qty);
    const double denom = bid_qty + ask_qty;
    if (denom > 0.0) {
      row.microprice = ((bid_px * ask_qty) + (ask_px * bid_qty)) / denom;
      row.imbalance = (bid_qty - ask_qty) / denom;
    }
  }

  return row;
}

}  // namespace lobster
