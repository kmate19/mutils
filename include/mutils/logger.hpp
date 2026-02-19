#pragma once

#include <format>
#include <functional>
#include <iostream>
#include <thread>

#ifdef _WIN32
#include <io.h>
#define ISATTY _isatty
#define FILENO _fileno
#else
#include <unistd.h>
#define ISATTY isatty
#define FILENO fileno
#endif

#ifndef LOG
#define LOG mutils::Logger::get().log
#endif

#ifndef LOG_ERR
#define LOG_ERR mutils::Logger::get().err
#endif

#ifndef LOG_WARN
#define LOG_WARN mutils::Logger::get().warn
#endif

#ifndef LOG_DBG
#ifndef NDEBUG
#define LOG_DBG mutils::Logger::get().dbg
#else
#define LOG_DBG(...) ((void)0)
#endif
#endif

namespace mutils {
class Logger {
public:
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;
  Logger(Logger &&) = delete;
  Logger &operator=(Logger &&) = delete;

  template <typename... Args>
  inline void log(std::format_string<Args...> fmt, Args &&...fmt_args) const {
    auto out = std::format(fmt, std::forward<Args>(fmt_args)...);

    std::cout << thread_color_ << "[THREAD " << thread_id_ << "] " << log_color_
              << "[LOG]: " << out << reset_ << "\n";
  }

  template <typename... Args>
  inline void err(std::format_string<Args...> fmt, Args &&...fmt_args) const {
    auto out = std::format(fmt, std::forward<Args>(fmt_args)...);

    std::cerr << thread_color_ << "[THREAD " << thread_id_ << "] "
              << error_color_ << "[ERROR]: " << out << reset_ << "\n";
  }

  template <typename... Args>
  inline void warn(std::format_string<Args...> fmt, Args &&...fmt_args) const {
    auto out = std::format(fmt, std::forward<Args>(fmt_args)...);

    std::cerr << thread_color_ << "[THREAD " << thread_id_ << "] "
              << warn_color_ << "[WARNING]: " << out << reset_ << "\n";
  }

  inline static void print_build_info() {
    std::cout << "=== Build Information ===\n";

    // Build type
    if constexpr (IS_DEBUG_BUILD) {
      std::cout << "Build Type: DEBUG\n";
    } else {
      std::cout << "Build Type: RELEASE\n";
    }

    // Compiler information
    std::cout << "Compiler: ";
#if defined(__clang__)
    std::cout << "Clang " << __clang_major__ << "." << __clang_minor__ << "."
              << __clang_patchlevel__ << "\n";
#elif defined(__GNUC__) || defined(__GNUG__)
    std::cout << "GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "."
              << __GNUC_PATCHLEVEL__ << "\n";
#elif defined(_MSC_VER)
    std::cout << "MSVC " << _MSC_VER << "\n";
#else
    std::cout << "Unknown\n";
#endif

    // C++ Standard
    std::cout << "C++ Standard: ";
#if __cplusplus == 202302L
    std::cout << "C++23\n";
#elif __cplusplus == 202002L
    std::cout << "C++20\n";
#elif __cplusplus == 201703L
    std::cout << "C++17\n";
#elif __cplusplus == 201402L
    std::cout << "C++14\n";
#elif __cplusplus == 201103L
    std::cout << "C++11\n";
#else
    std::cout << "Pre-C++11 or unknown (" << __cplusplus << ")\n";
#endif

    // Platform
    std::cout << "Platform: ";
#if defined(_WIN32) || defined(_WIN64)
    std::cout << "Windows";
#ifdef _WIN64
    std::cout << " (64-bit)";
#else
    std::cout << " (32-bit)";
#endif
    std::cout << "\n";
#elif defined(__APPLE__) || defined(__MACH__)
    std::cout << "macOS\n";
#elif defined(__linux__)
    std::cout << "Linux\n";
#elif defined(__unix__)
    std::cout << "Unix\n";
#elif defined(__FreeBSD__)
    std::cout << "FreeBSD\n";
#else
    std::cout << "Unknown\n";
#endif

    // Architecture
    std::cout << "Architecture: ";
#if defined(__x86_64__) || defined(_M_X64)
    std::cout << "x86_64\n";
#elif defined(__i386__) || defined(_M_IX86)
    std::cout << "x86\n";
#elif defined(__aarch64__) || defined(_M_ARM64)
    std::cout << "ARM64\n";
#elif defined(__arm__) || defined(_M_ARM)
    std::cout << "ARM\n";
#else
    std::cout << "Unknown\n";
#endif

    // Optimizations
    std::cout << "Optimizations: ";
#if defined(__OPTIMIZE__)
    std::cout << "Enabled";
#if defined(__OPTIMIZE_SIZE__)
    std::cout << " (Size)";
#endif
    std::cout << "\n";
#else
    std::cout << "Disabled\n";
#endif

    // Assertions
    std::cout << "Assertions: ";
#ifdef NDEBUG
    std::cout << "Disabled\n";
#else
    std::cout << "Enabled\n";
#endif

    // Additional compiler flags
    std::cout << "Additional Features:\n";

#ifdef __SSE__
    std::cout << "  - SSE: Enabled\n";
#endif
#ifdef __SSE2__
    std::cout << "  - SSE2: Enabled\n";
#endif
#ifdef __AVX__
    std::cout << "  - AVX: Enabled\n";
#endif
#ifdef __AVX2__
    std::cout << "  - AVX2: Enabled\n";
#endif

#ifdef _OPENMP
    std::cout << "  - OpenMP: Enabled\n";
#endif

#ifdef __cpp_exceptions
    std::cout << "  - Exceptions: Enabled\n";
#else
    std::cout << "  - Exceptions: Disabled\n";
#endif

#ifdef __cpp_rtti
    std::cout << "  - RTTI: Enabled\n";
#else
    std::cout << "  - RTTI: Disabled\n";
#endif

    // Compile time and date
    std::cout << "Compiled: " << __DATE__ << " at " << __TIME__ << "\n";

    std::cout << "=========================\n";
  }

  static Logger &get() {
    thread_local Logger instance{};
    return instance;
  }

  template <typename... Args>
  inline void dbg(std::format_string<Args...> fmt, Args &&...fmt_args) const {
    if constexpr (IS_DEBUG_BUILD) {
      auto out = std::format(fmt, std::forward<Args>(fmt_args)...);

      std::cout << thread_color_ << "[THREAD " << thread_id_ << "] "
                << log_color_ << "[DEBUG]: " << out << reset_ << "\n";
    }
  }

private:
  Logger() : thread_id_(std::this_thread::get_id()) {
    // Check if stdout/stderr are connected to a terminal
    bool stdout_is_tty = ISATTY(FILENO(stdout));
    bool stderr_is_tty = ISATTY(FILENO(stderr));

    bool is_tty = stdout_is_tty || stderr_is_tty;

    if (is_tty) {
      thread_color_ = compute_thread_color();
      log_color_ = "\033[32m";   // Green
      warn_color_ = "\033[33m";  // Yellow
      error_color_ = "\033[31m"; // Red
      reset_ = "\033[0m";
    }
  }

  const char *compute_thread_color() const {
    size_t hash = std::hash<std::thread::id>{}(thread_id_);

    static const char *colors[] = {
        "\033[96m", // Bright Cyan
        "\033[95m", // Bright Magenta
        "\033[94m", // Bright Blue
        "\033[93m", // Bright Yellow
        "\033[92m", // Bright Green
        "\033[91m", // Bright Red
        "\033[36m", // Cyan
        "\033[35m", // Magenta
        "\033[34m", // Blue
    };

    return colors[hash % 9];
  }

  static constexpr bool IS_DEBUG_BUILD =
#ifdef NDEBUG
      false;
#else
      true;
#endif

  std::thread::id thread_id_;
  const char *thread_color_ = "";
  const char *log_color_ = "";
  const char *warn_color_ = "";
  const char *error_color_ = "";
  const char *reset_ = "";
}; // namespace myproj
} // namespace mutils
