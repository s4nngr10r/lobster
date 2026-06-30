#include <cassert>
#include <cstddef>

#include "lobster/transport.hpp"

int main() {
  const auto header = lobster::make_header();
  assert(lobster::header_valid(header));
  assert(lobster::as_bytes(header).size() == sizeof(lobster::FileHeader));
  const auto footer = lobster::make_footer(3);
  assert(lobster::footer_valid(footer));
  assert(lobster::as_bytes(footer).size() == sizeof(lobster::FileFooter));
  assert(footer.record_count == 3);

  const lobster::BookEvent event{
      .ts_ns = 7,
      .seq = 9,
      .source_first_id = 8,
      .source_last_id = 9,
      .price_ticks = 12345,
      .qty = 11,
      .side = lobster::Side::ask,
      .kind = lobster::EventKind::book_level,
  };

  const auto wire = lobster::encode_event(event);
  const auto decoded = lobster::decode_event(wire);

  assert(decoded.ts_ns == event.ts_ns);
  assert(decoded.seq == event.seq);
  assert(decoded.source_first_id == event.source_first_id);
  assert(decoded.source_last_id == event.source_last_id);
  assert(decoded.price_ticks == event.price_ticks);
  assert(decoded.qty == event.qty);
  assert(decoded.side == event.side);
  assert(decoded.kind == event.kind);
  return 0;
}
