#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "lobster/types.hpp"

namespace lobster {

inline constexpr std::array<char, 4> k_magic{'L', 'B', 'J', 'R'};
inline constexpr std::array<char, 4> k_footer_magic{'L', 'B', 'F', 'T'};
inline constexpr std::uint16_t k_version = 1;

struct FileHeader {
  char magic[4];
  std::uint16_t version;
  std::uint16_t header_bytes;
};

struct WireEvent {
  std::uint64_t ts_ns;
  std::uint64_t seq;
  std::uint64_t source_first_id;
  std::uint64_t source_last_id;
  std::int64_t price_ticks;
  std::uint64_t qty;
  std::uint8_t side;
  std::uint8_t kind;
  std::uint16_t reserved0;
  std::uint32_t reserved1;
};

static_assert(sizeof(WireEvent) == 56, "WireEvent size must stay fixed");

struct FileFooter {
  char magic[4];
  std::uint32_t footer_bytes;
  std::uint64_t record_count;
};

static_assert(sizeof(FileFooter) == 16, "FileFooter size must stay fixed");

FileHeader make_header();
FileFooter make_footer(std::uint64_t record_count);
bool header_valid(const FileHeader& header);
bool footer_valid(const FileFooter& footer);
WireEvent encode_event(const BookEvent& event);
BookEvent decode_event(const WireEvent& wire);
std::span<const std::byte> as_bytes(const FileHeader& header);
std::span<const std::byte> as_bytes(const WireEvent& event);
std::span<const std::byte> as_bytes(const FileFooter& footer);

}  // namespace lobster
