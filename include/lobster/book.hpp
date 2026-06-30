#pragma once

#include <cstdint>
#include <map>

#include "lobster/types.hpp"

namespace lobster {

class OrderBook {
 public:
  void apply(const BookEvent& event);
  TopOfBook top_of_book() const;
  DepthLevels bid_depth_levels() const;
  DepthLevels ask_depth_levels() const;
  std::size_t bid_levels() const;
  std::size_t ask_levels() const;

 private:
  std::map<std::int64_t, std::uint64_t, std::greater<>> bids_;
  std::map<std::int64_t, std::uint64_t, std::less<>> asks_;
};

}  // namespace lobster
