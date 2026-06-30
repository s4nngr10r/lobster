#include "lobster/bootstrap.hpp"

namespace lobster {

namespace {

void validate_event_shape(const BookEvent& event) {
  if (event.side != Side::bid && event.side != Side::ask) {
    throw BootstrapError("invalid side in capture event");
  }
}

template <typename Map>
void upsert_snapshot(Map& levels, const BookEvent& event) {
  if (event.qty == 0) {
    levels.erase(event.price_ticks);
    return;
  }
  levels[event.price_ticks] = event;
}

}  // namespace

void SnapshotBootstrap::on_snapshot(const BookEvent& event) {
  if (synced_) {
    throw BootstrapError("snapshot event after sync");
  }
  validate_event_shape(event);
  BookEvent snapshot = event;
  snapshot.kind = EventKind::snapshot_level;

  if (snapshot.side == Side::bid) {
    upsert_snapshot(snapshot_bids_, snapshot);
    return;
  }
  upsert_snapshot(snapshot_asks_, snapshot);
}

void SnapshotBootstrap::on_incremental(const BookEvent& event, const Handler& emit) {
  validate_event_shape(event);
  BookEvent live = event;
  live.kind = EventKind::book_level;

  if (!synced_) {
    if (have_buffered_seq_ && live.seq <= last_buffered_seq_) {
      throw BootstrapError("incrementals must arrive in ascending order before sync");
    }
    buffered_updates_.push_back(live);
    last_buffered_seq_ = live.seq;
    have_buffered_seq_ = true;
    return;
  }

  if (live.seq != last_live_seq_ + 1) {
    throw BootstrapError("gap after sync");
  }
  emit(live);
  last_live_seq_ = live.seq;
}

void SnapshotBootstrap::sync_to(std::uint64_t snapshot_seq, const Handler& emit) {
  if (synced_) {
    throw BootstrapError("sync already completed");
  }

  for (const auto& [_, event] : snapshot_bids_) {
    emit(event);
  }
  for (const auto& [_, event] : snapshot_asks_) {
    emit(event);
  }

  std::uint64_t expected_seq = snapshot_seq + 1;
  for (const auto& event : buffered_updates_) {
    // Drop incrementals already covered by the snapshot before replaying the buffered tail.
    if (event.seq <= snapshot_seq) {
      continue;
    }
    if (event.seq != expected_seq) {
      throw BootstrapError("gap between snapshot and buffered incrementals");
    }
    emit(event);
    last_live_seq_ = event.seq;
    expected_seq = event.seq + 1;
  }

  if (last_live_seq_ == 0) {
    last_live_seq_ = snapshot_seq;
  }
  synced_ = true;
}

bool SnapshotBootstrap::synced() const { return synced_; }

}  // namespace lobster
