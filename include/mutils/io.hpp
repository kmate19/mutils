#pragma once

#include "logger.hpp"
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace mutils {
// Reads the entire contents of a file into a vector of chars. Returns
// std::nullopt on failure.
inline std::optional<std::vector<char>> readFile(const std::string &filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    LOG_ERR("Failed to open file: {} - {}", filename, strerror(errno));
    return std::nullopt;
  }
  size_t fileSize = static_cast<size_t>(file.tellg());
  std::vector<char> buffer(fileSize);
  file.seekg(0);
  file.read(buffer.data(), fileSize);
  file.close();
  return buffer;
}

// Reads the entire contents of a file into a string. Returns std::nullopt on
// failure.
inline std::optional<std::string>
readFileToString(const std::string &filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    LOG_ERR("Failed to open file: {} - {}", filename, strerror(errno));
    return std::nullopt;
  }
  std::string buffer(static_cast<size_t>(file.tellg()), '\0');
  file.seekg(0);
  file.read(buffer.data(), buffer.size());
  return buffer;
}

// A simple range class to iterate over lines in a string without copying
class LineRange {
public:
  explicit LineRange(const std::string &str) : str_(str) {}

  struct Iterator {
    const std::string &str;
    size_t pos;
    std::string current;

    Iterator(const std::string &str, size_t pos) : str(str), pos(pos) {
      advance();
    }

    void advance() {
      if (pos >= str.size()) {
        pos = std::string::npos;
        return;
      }
      size_t end = str.find('\n', pos);
      if (end == std::string::npos) {
        current = str.substr(pos);
        pos = std::string::npos;
      } else {
        current = str.substr(pos, end - pos);
        pos = end + 1;
      }
    }

    const std::string &operator*() const { return current; }
    Iterator &operator++() {
      advance();
      return *this;
    }
    bool operator!=(const Iterator &other) const { return pos != other.pos; }
  };

  Iterator begin() const { return Iterator(str_, 0); }
  Iterator end() const { return Iterator(str_, std::string::npos); }

private:
  const std::string &str_;
};

// Returns a LineRange that can be used to iterate over lines in the input
// string
inline LineRange lines(const std::string &str) { return LineRange(str); }

} // namespace mutils
