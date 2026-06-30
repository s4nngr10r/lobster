#include "lobster/journal.hpp"

#include <filesystem>
#include <stdexcept>

namespace lobster {

JournalReader::JournalReader(const std::filesystem::path& path)
    : stream_(path, std::ios::binary) {
  if (!stream_) {
    throw std::runtime_error("failed to open journal for reading");
  }

  FileHeader header{};
  stream_.read(reinterpret_cast<char*>(&header),
               static_cast<std::streamsize>(sizeof(header)));
  if (!stream_ || !header_valid(header)) {
    throw std::runtime_error("invalid journal header");
  }

  const auto file_size = std::filesystem::file_size(path);
  if (file_size < sizeof(FileHeader) + sizeof(FileFooter)) {
    throw std::runtime_error("journal missing footer");
  }
  if ((file_size - sizeof(FileHeader) - sizeof(FileFooter)) % sizeof(WireEvent) != 0) {
    throw std::runtime_error("journal body has partial record bytes");
  }

  stream_.seekg(-static_cast<std::streamoff>(sizeof(FileFooter)), std::ios::end);
  FileFooter footer{};
  stream_.read(reinterpret_cast<char*>(&footer),
               static_cast<std::streamsize>(sizeof(footer)));
  if (!stream_ || !footer_valid(footer)) {
    throw std::runtime_error("invalid journal footer");
  }

  const auto body_record_count =
      (file_size - sizeof(FileHeader) - sizeof(FileFooter)) / sizeof(WireEvent);
  if (body_record_count != footer.record_count) {
    throw std::runtime_error("journal footer record count mismatch");
  }
  expected_record_count_ = footer.record_count;
  stream_.clear();
  stream_.seekg(static_cast<std::streamoff>(sizeof(FileHeader)), std::ios::beg);
}

std::optional<BookEvent> JournalReader::next() {
  if (records_read_ >= expected_record_count_) {
    return std::nullopt;
  }
  WireEvent wire{};
  stream_.read(reinterpret_cast<char*>(&wire),
               static_cast<std::streamsize>(sizeof(wire)));
  if (!stream_) {
    throw std::runtime_error("truncated journal record");
  }
  ++records_read_;
  return decode_event(wire);
}

}  // namespace lobster
