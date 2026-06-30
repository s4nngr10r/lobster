#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <stdexcept>

#include "lobster/journal.hpp"

namespace lobster {

class ReplayError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class ReplayEngine {
 public:
  using Handler = std::function<void(const BookEvent&)>;

  void replay(const std::filesystem::path& path, const Handler& handler) const;
};

}  // namespace lobster
