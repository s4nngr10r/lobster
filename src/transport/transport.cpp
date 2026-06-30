#include "lobster/transport.hpp"

#include <bit>

namespace lobster {

namespace {

template <typename T>
std::span<const std::byte> bytes_of(const T& value) {
  return std::as_bytes(std::span{&value, 1});
}

}  // namespace

FileHeader make_header() {
  return FileHeader{
      .magic = {k_magic[0], k_magic[1], k_magic[2], k_magic[3]},
      .version = k_version,
      .header_bytes = static_cast<std::uint16_t>(sizeof(FileHeader)),
  };
}

FileFooter make_footer(std::uint64_t record_count) {
  return FileFooter{
      .magic = {k_footer_magic[0], k_footer_magic[1], k_footer_magic[2],
                k_footer_magic[3]},
      .footer_bytes = static_cast<std::uint32_t>(sizeof(FileFooter)),
      .record_count = record_count,
  };
}

bool header_valid(const FileHeader& header) {
  if (std::endian::native != std::endian::little) {
    return false;
  }
  return header.magic[0] == k_magic[0] && header.magic[1] == k_magic[1] &&
         header.magic[2] == k_magic[2] && header.magic[3] == k_magic[3] &&
         header.version == k_version &&
         header.header_bytes == sizeof(FileHeader);
}

bool footer_valid(const FileFooter& footer) {
  return footer.magic[0] == k_footer_magic[0] &&
         footer.magic[1] == k_footer_magic[1] &&
         footer.magic[2] == k_footer_magic[2] &&
         footer.magic[3] == k_footer_magic[3] &&
         footer.footer_bytes == sizeof(FileFooter);
}

WireEvent encode_event(const BookEvent& event) {
  return WireEvent{
      .ts_ns = event.ts_ns,
      .seq = event.seq,
      .source_first_id = event.source_first_id,
      .source_last_id = event.source_last_id,
      .price_ticks = event.price_ticks,
      .qty = event.qty,
      .side = static_cast<std::uint8_t>(event.side),
      .kind = static_cast<std::uint8_t>(event.kind),
      .reserved0 = 0,
      .reserved1 = 0,
  };
}

BookEvent decode_event(const WireEvent& wire) {
  return BookEvent{
      .ts_ns = wire.ts_ns,
      .seq = wire.seq,
      .source_first_id = wire.source_first_id,
      .source_last_id = wire.source_last_id,
      .price_ticks = wire.price_ticks,
      .qty = wire.qty,
      .side = static_cast<Side>(wire.side),
      .kind = static_cast<EventKind>(wire.kind),
  };
}

std::span<const std::byte> as_bytes(const FileHeader& header) {
  return bytes_of(header);
}

std::span<const std::byte> as_bytes(const WireEvent& event) {
  return bytes_of(event);
}

std::span<const std::byte> as_bytes(const FileFooter& footer) {
  return bytes_of(footer);
}

}  // namespace lobster
