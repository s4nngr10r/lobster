#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <stdexcept>
#include <vector>

#include "lobster/types.hpp"

namespace lobster {

class BootstrapError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class SnapshotBootstrap {
 public:
  using Handler = std::function<void(const BookEvent&)>;

  void on_snapshot(const BookEvent& event);
  void on_incremental(const BookEvent& event, const Handler& emit);
  void sync_to(std::uint64_t snapshot_seq, const Handler& emit);
  bool synced() const;

 private:
  std::map<std::int64_t, BookEvent, std::greater<>> snapshot_bids_;
  std::map<std::int64_t, BookEvent, std::less<>> snapshot_asks_;
  std::vector<BookEvent> buffered_updates_;
  std::uint64_t last_buffered_seq_{0};
  std::uint64_t last_live_seq_{0};
  bool have_buffered_seq_{false};
  bool synced_{false};
};

}  // namespace lobster
