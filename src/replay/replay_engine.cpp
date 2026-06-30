#include "lobster/replay.hpp"

namespace lobster {

namespace {

bool event_valid(const BookEvent& event) {
  const bool side_valid = event.side == Side::bid || event.side == Side::ask;
  const bool kind_valid = event.kind == EventKind::book_level ||
                          event.kind == EventKind::snapshot_level ||
                          event.kind == EventKind::trade ||
                          event.kind == EventKind::heartbeat ||
                          event.kind == EventKind::venue_status;
  return side_valid && kind_valid;
}

}  // namespace

void ReplayEngine::replay(const std::filesystem::path& path,
                          const Handler& handler) const {
  JournalReader reader(path);
  std::uint64_t last_seq = 0;
  bool first = true;
  EventKind last_kind = EventKind::book_level;

  while (const auto event = reader.next()) {
    if (!event_valid(*event)) {
      throw ReplayError("invalid event in replay");
    }
    if (!first && event->seq < last_seq) {
      throw ReplayError("out-of-order sequence in replay");
    }
    if (!first && event->seq == last_seq &&
        !(last_kind == EventKind::snapshot_level &&
          event->kind == EventKind::snapshot_level)) {
      throw ReplayError("duplicate live sequence in replay");
    }
    if (!first && event->seq != last_seq &&
        !(last_kind == EventKind::snapshot_level &&
          event->kind == EventKind::snapshot_level) &&
        event->seq != last_seq + 1) {
      throw ReplayError("gap in replay stream");
    }
    first = false;
    last_seq = event->seq;
    last_kind = event->kind;
    handler(*event);
  }
}

}  // namespace lobster
