#pragma once

#include "logger.hpp"
#include <chrono>

namespace mutils {
class Timer {
public:
  Timer() : start_(std::chrono::high_resolution_clock::now()) {}

  // Resets the timer to the current time
  void reset() { start_ = std::chrono::high_resolution_clock::now(); }

  // Returns elapsed time in microseconds
  long long elapsedUs() const {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::high_resolution_clock::now() - start_)
        .count();
  }

  // Returns elapsed time in milliseconds
  double elapsedMs() const {
    return std::chrono::duration<double, std::milli>(
               std::chrono::high_resolution_clock::now() - start_)
        .count();
  }

  // Returns elapsed time in seconds
  double elapsedSec() const {
    return std::chrono::duration<double>(
               std::chrono::high_resolution_clock::now() - start_)
        .count();
  }

  // Logs the elapsed time in milliseconds with an optional label
  void printElapsed(const std::string &label = "Elapsed time") const {
    LOG("{}: {:.3f} ms", label, elapsedMs());
  }

private:
  std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};
} // namespace mutils
