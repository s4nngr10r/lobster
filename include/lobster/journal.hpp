#pragma once

#include <filesystem>
#include <fstream>
#include <optional>

#include "lobster/transport.hpp"

namespace lobster {

class JournalWriter {
 public:
  explicit JournalWriter(const std::filesystem::path& path);
  ~JournalWriter();
  void append(const BookEvent& event);
  void close();

 private:
  bool closed_{false};
  std::uint64_t record_count_{0};
  std::ofstream stream_;
};

class JournalReader {
 public:
  explicit JournalReader(const std::filesystem::path& path);
  std::optional<BookEvent> next();

 private:
  std::uint64_t expected_record_count_{0};
  std::uint64_t records_read_{0};
  std::ifstream stream_;
};

}  // namespace lobster
