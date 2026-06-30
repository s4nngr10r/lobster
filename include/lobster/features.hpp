#pragma once

#include <cstdint>
#include <optional>

#include "lobster/types.hpp"

namespace lobster {

struct TopOfBookFeatures {
  std::uint64_t ts_ns{};
  std::uint64_t seq{};
  std::uint64_t source_first_id{};
  std::uint64_t source_last_id{};
  std::optional<std::int64_t> best_bid_price{};
  std::optional<std::uint64_t> best_bid_qty{};
  std::optional<std::int64_t> best_ask_price{};
  std::optional<std::uint64_t> best_ask_qty{};
  std::optional<std::uint64_t> bid_depth_1{};
  std::optional<std::uint64_t> ask_depth_1{};
  std::optional<std::uint64_t> bid_depth_5{};
  std::optional<std::uint64_t> ask_depth_5{};
  std::optional<std::uint64_t> bid_depth_10{};
  std::optional<std::uint64_t> ask_depth_10{};
  std::uint64_t bid_levels{};
  std::uint64_t ask_levels{};
  std::optional<std::int64_t> spread_ticks{};
  std::optional<double> mid_price{};
  std::optional<double> microprice{};
  std::optional<double> imbalance{};
};

TopOfBookFeatures compute_top_of_book_features(const BookEvent& event, const TopOfBook& top,
                                               const DepthLevels& bid_depths,
                                               const DepthLevels& ask_depths,
                                               std::size_t bid_levels,
                                               std::size_t ask_levels);

}  // namespace lobster
