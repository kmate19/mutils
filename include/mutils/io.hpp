#pragma once

#include "logger.hpp"

#include <cstddef>
#include <fstream>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace mutils {

template <std::ranges::contiguous_range Rng> inline char *as_chars(Rng &rng) {
  return reinterpret_cast<char *>(std::ranges::data(rng));
}

template <std::ranges::contiguous_range Rng>
inline const char *as_chars(Rng const &rng) {
  return reinterpret_cast<const char *>(std::ranges::data(rng));
}

// Reads the entire contents of a file into a vector of chars. Returns
// std::nullopt on failure.
inline std::optional<std::vector<std::byte>>
readFile(const std::string &filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    LOG_ERR("Failed to open file: {} - {}", filename,
            std::system_category().message(errno));
    return std::nullopt;
  }
  size_t fileSize = static_cast<size_t>(file.tellg());
  std::vector<std::byte> buffer(fileSize);
  file.seekg(0);
  file.read(as_chars(buffer), fileSize);
  if (file.fail()) {
    LOG_ERR("Failed to read file: {} - {}", filename,
            std::system_category().message(errno));
    return std::nullopt;
  }
  file.close();
  return buffer;
}

// Reads the entire contents of a file into a string. Returns std::nullopt on
// failure.
inline std::optional<std::string>
readFileToString(const std::string &filename) {
  std::ifstream file(filename, std::ios::ate);
  if (!file.is_open()) {
    LOG_ERR("Failed to open file: {} - {}", filename,
            std::system_category().message(errno));
    return std::nullopt;
  }
  std::string buffer(static_cast<size_t>(file.tellg()), '\0');
  file.seekg(0);
  file.read(buffer.data(), buffer.size());
  if (file.fail()) {
    LOG_ERR("Failed to read file: {} - {}", filename,
            std::system_category().message(errno));
    return std::nullopt;
  }
  return buffer;
}

// A simple range class to iterate over lines in a string without copying
class LineRange {
public:
  explicit LineRange(const std::string_view str) : str_(str) {}

  struct Iterator {
    const std::string_view str_;
    size_t pos_;
    std::string_view current_;

    Iterator(const std::string_view str, size_t pos) : str_(str), pos_(pos) {
      advance();
    }

    void advance() {
      if (pos_ == std::string_view::npos || pos_ >= str_.size()) {
        pos_ = std::string_view::npos;
        return;
      }
      size_t end = str_.find('\n', pos_);
      if (end == std::string_view::npos) {
        current_ = str_.substr(pos_);
        pos_ = std::string_view::npos;
      } else {
        current_ = str_.substr(pos_, end - pos_);
        pos_ = end + 1;
      }
    }

    std::string_view operator*() const { return current_; }
    Iterator &operator++() {
      advance();
      return *this;
    }
    bool operator!=(const Iterator &other) const { return pos_ != other.pos_; }
  };

  Iterator begin() const { return Iterator(str_, 0); }
  Iterator end() const { return Iterator(str_, std::string_view::npos); }

private:
  const std::string_view str_;
};

// Returns a LineRange that can be used to iterate over lines in the input
// string
inline LineRange lines(const std::string_view str) { return LineRange(str); }

} // namespace mutils
