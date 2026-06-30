#include "lobster/journal.hpp"

#include <stdexcept>

namespace lobster {

JournalWriter::JournalWriter(const std::filesystem::path& path)
    : stream_(path, std::ios::binary | std::ios::trunc) {
  if (!stream_) {
    throw std::runtime_error("failed to open journal for writing");
  }

  const auto header = make_header();
  const auto bytes = as_bytes(header);
  stream_.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
  if (!stream_) {
    throw std::runtime_error("failed to write journal header");
  }
}

JournalWriter::~JournalWriter() {
  try {
    close();
  } catch (...) {
  }
}

void JournalWriter::append(const BookEvent& event) {
  if (closed_) {
    throw std::runtime_error("cannot append to closed journal");
  }
  const auto wire = encode_event(event);
  const auto bytes = as_bytes(wire);
  stream_.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
  if (!stream_) {
    throw std::runtime_error("failed to append journal record");
  }
  ++record_count_;
}

void JournalWriter::close() {
  if (closed_) {
    return;
  }
  const auto footer = make_footer(record_count_);
  const auto bytes = as_bytes(footer);
  stream_.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
  if (!stream_) {
    throw std::runtime_error("failed to write journal footer");
  }
  stream_.flush();
  if (!stream_) {
    throw std::runtime_error("failed to flush journal footer");
  }
  closed_ = true;
}

}  // namespace lobster
