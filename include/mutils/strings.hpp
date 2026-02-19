#pragma once

#include <sstream>
#include <string>
#include <vector>

namespace mutils {
// Trim leading and trailing whitespace from a string
inline std::string trim(const std::string &s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  size_t end = s.find_last_not_of(" \t\r\n");
  return start == std::string::npos ? "" : s.substr(start, end - start + 1);
}

// Split a string by a delimiter and return a vector of trimmed tokens
inline std::vector<std::string> split(const std::string &s, char delim) {
  std::vector<std::string> result;
  std::stringstream ss(s);
  std::string token;
  while (std::getline(ss, token, delim))
    result.push_back(token);
  return result;
}

// Check if a string starts with a given prefix
inline bool startsWith(const std::string &s, const std::string &prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

// Check if a string ends with a given suffix
inline bool endsWith(const std::string &s, const std::string &suffix) {
  return s.size() >= suffix.size() &&
         s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}
} // namespace mutils
