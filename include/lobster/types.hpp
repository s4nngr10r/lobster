#pragma once

#include <cstdint>
#include <optional>

namespace lobster {

enum class Side : std::uint8_t {
  bid = 1,
  ask = 2,
};

enum class EventKind : std::uint8_t {
  book_level = 1,
  snapshot_level = 2,
  trade = 3,
  heartbeat = 4,
  venue_status = 5,
};

struct BookEvent {
  std::uint64_t ts_ns{};
  std::uint64_t seq{};
  std::uint64_t source_first_id{};
  std::uint64_t source_last_id{};
  std::int64_t price_ticks{};
  std::uint64_t qty{};
  Side side{Side::bid};
  EventKind kind{EventKind::book_level};
};

struct TopOfBook {
  std::optional<std::int64_t> best_bid_price{};
  std::optional<std::uint64_t> best_bid_qty{};
  std::optional<std::int64_t> best_ask_price{};
  std::optional<std::uint64_t> best_ask_qty{};
};

struct DepthLevels {
  std::optional<std::uint64_t> depth_1{};
  std::optional<std::uint64_t> depth_5{};
  std::optional<std::uint64_t> depth_10{};
};

}  // namespace lobster
