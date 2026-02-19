#pragma once

#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <sstream>
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

#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)

namespace mutils {

struct LogSink {
  std::mutex mtx;
  std::ofstream file;

  static LogSink &get() {
    static LogSink instance;
    return instance;
  }

  // Call once before spawning threads. Safe to call multiple times;
  // subsequent calls reopen the file (truncating unless append=true).
  bool open(const std::filesystem::path &path, bool append = false) {
    std::lock_guard lock(mtx);
    if (file.is_open()) {
      file.close();
    }
    auto mode = std::ios::out | (append ? std::ios::app : std::ios::trunc);
    file.open(path, mode);
    return file.is_open();
  }

  void close() {
    std::lock_guard lock(mtx);
    if (file.is_open()) {
      file.close();
    }
  }

  bool is_open() const { return file.is_open(); }

  // Write to file (must be called with mtx held).
  // Strips ANSI escape sequences so the file stays clean.
  void write_to_file(std::string_view msg) {
    // Simple state-machine ANSI stripper
    bool in_escape = false;
    for (char c : msg) {
      if (in_escape) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
          in_escape = false;
      } else if (c == '\033') {
        in_escape = true;
      } else {
        file << c;
      }
    }
    file << '\n';
  }

private:
  LogSink() = default;
};

class Logger {
public:
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;
  Logger(Logger &&) = delete;
  Logger &operator=(Logger &&) = delete;

  static bool init_file(const std::filesystem::path &path,
                        bool append = false) {
    return LogSink::get().open(path, append);
  }

  static void close_file() { LogSink::get().close(); }

  template <typename... Args>
  inline void log(std::format_string<Args...> fmt, Args &&...fmt_args) const {
    auto body = std::format(fmt, std::forward<Args>(fmt_args)...);
    auto msg = thread_color_ + std::format("[THREAD {}] ", thread_id_str_) +
               log_color_ + std::string("[LOG]: ") + body + reset_;
    write(msg, /*flush=*/false);
  }

  template <typename... Args>
  inline void err(std::format_string<Args...> fmt, Args &&...fmt_args) const {
    auto body = std::format(fmt, std::forward<Args>(fmt_args)...);
    auto msg = thread_color_ + std::format("[THREAD {}] ", thread_id_str_) +
               error_color_ + std::string("[ERROR]: ") + body + reset_;
    write(msg, /*flush=*/true, /*use_stderr=*/true);
  }

  template <typename... Args>
  inline void warn(std::format_string<Args...> fmt, Args &&...fmt_args) const {
    auto body = std::format(fmt, std::forward<Args>(fmt_args)...);
    auto msg = thread_color_ + std::format("[THREAD {}] ", thread_id_str_) +
               warn_color_ + std::string("[WARNING]: ") + body + reset_;
    write(msg, /*flush=*/false, /*use_stderr=*/true);
  }

  inline static void print_build_info() {
    static_write("=== Build Information ===", /*flush=*/true);
    // Build type
    if constexpr (IS_DEBUG_BUILD) {
      static_write("Build Type: DEBUG", /*flush=*/true);
    } else {
      static_write("Build Type: RELEASE", /*flush=*/true);
    }
    // Compiler information
#if defined(__clang__)
    static_write("Compiler: Clang " STRINGIFY(__clang_major__) "." STRINGIFY(
                     __clang_minor__) "." STRINGIFY(__clang_patchlevel__),
                 /*flush=*/true);
#elif defined(__GNUC__) || defined(__GNUG__)
    static_write("Compiler: GCC " STRINGIFY(__GNUC__) "." STRINGIFY(
                     __GNUC_MINOR__) "." STRINGIFY(__GNUC_PATCHLEVEL__),
                 /*flush=*/true);
#elif defined(_MSC_VER)
    static_write("Compiler: MSVC " STRINGIFY(_MSC_VER), /*flush=*/true);
#else
    static_write("Compiler: Unknown", /*flush=*/true);
#endif
    // C++ Standard
#if __cplusplus == 202302L
    static_write("C++ Standard: C++23", /*flush=*/true);
#elif __cplusplus == 202002L
    static_write("C++ Standard: C++20", /*flush=*/true);
#elif __cplusplus == 201703L
    static_write("C++ Standard: C++17", /*flush=*/true);
#elif __cplusplus == 201402L
    static_write("C++ Standard: C++14", /*flush=*/true);
#elif __cplusplus == 201103L
    static_write("C++ Standard: C++11", /*flush=*/true);
#else
    static_write(
        "C++ Standard: Pre-C++11 or unknown (" STRINGIFY(__cplusplus) ")",
        /*flush=*/true);
#endif
    // Platform
#if defined(_WIN32) || defined(_WIN64)
#ifdef _WIN64
    static_write("Platform: Windows (64-bit)", /*flush=*/true);
#else
    static_write("Platform: Windows (32-bit)", /*flush=*/true);
#endif
#elif defined(__APPLE__) || defined(__MACH__)
    static_write("Platform: macOS", /*flush=*/true);
#elif defined(__linux__)
    static_write("Platform: Linux", /*flush=*/true);
#elif defined(__unix__)
    static_write("Platform: Unix", /*flush=*/true);
#elif defined(__FreeBSD__)
    static_write("Platform: FreeBSD", /*flush=*/true);
#else
    static_write("Platform: Unknown", /*flush=*/true);
#endif
    // Architecture
#if defined(__x86_64__) || defined(_M_X64)
    static_write("Architecture: x86_64", /*flush=*/true);
#elif defined(__i386__) || defined(_M_IX86)
    static_write("Architecture: x86", /*flush=*/true);
#elif defined(__aarch64__) || defined(_M_ARM64)
    static_write("Architecture: ARM64", /*flush=*/true);
#elif defined(__arm__) || defined(_M_ARM)
    static_write("Architecture: ARM", /*flush=*/true);
#else
    static_write("Architecture: Unknown", /*flush=*/true);
#endif
    // Optimizations
#if defined(__OPTIMIZE__)
#if defined(__OPTIMIZE_SIZE__)
    static_write("Optimizations: Enabled (Size)", /*flush=*/true);
#else
    static_write("Optimizations: Enabled", /*flush=*/true);
#endif
#else
    static_write("Optimizations: Disabled", /*flush=*/true);
#endif
    // Assertions
#ifdef NDEBUG
    static_write("Assertions: Disabled", /*flush=*/true);
#else
    static_write("Assertions: Enabled", /*flush=*/true);
#endif
    // Additional compiler flags
    static_write("Additional Features:", /*flush=*/true);
#ifdef __SSE__
    static_write("  - SSE: Enabled", /*flush=*/true);
#endif
#ifdef __SSE2__
    static_write("  - SSE2: Enabled", /*flush=*/true);
#endif
#ifdef __AVX__
    static_write("  - AVX: Enabled", /*flush=*/true);
#endif
#ifdef __AVX2__
    static_write("  - AVX2: Enabled", /*flush=*/true);
#endif
#ifdef _OPENMP
    static_write("  - OpenMP: Enabled", /*flush=*/true);
#endif
#ifdef __cpp_exceptions
    static_write("  - Exceptions: Enabled", /*flush=*/true);
#else
    static_write("  - Exceptions: Disabled", /*flush=*/true);
#endif
#ifdef __cpp_rtti
    static_write("  - RTTI: Enabled", /*flush=*/true);
#else
    static_write("  - RTTI: Disabled", /*flush=*/true);
#endif
    // Compile time and date
    static_write("Compiled: " __DATE__ " at " __TIME__, /*flush=*/true);
    static_write("=========================", /*flush=*/true);
  }

  static Logger &get() {
    thread_local Logger instance{};
    return instance;
  }

  template <typename... Args>
  inline void dbg(std::format_string<Args...> fmt, Args &&...fmt_args) const {
    if constexpr (IS_DEBUG_BUILD) {
      auto body = std::format(fmt, std::forward<Args>(fmt_args)...);
      auto msg = thread_color_ + std::format("[THREAD {}] ", thread_id_str_) +
                 log_color_ + std::string("[DEBUG]: ") + body + reset_;
      write(msg, /*flush=*/false);
    }
  }

private:
  Logger() : thread_id_(std::this_thread::get_id()) {
    std::ostringstream oss;
    oss << thread_id_;
    thread_id_str_ = oss.str();
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

  void write(const std::string &msg, bool flush,
             bool use_stderr = false) const {
    auto &sink = LogSink::get();
    std::lock_guard lock(sink.mtx);

    auto &stream = use_stderr ? std::cerr : std::cout;
    stream << msg << '\n';

    if (sink.file.is_open()) {
      sink.write_to_file(msg);
      if (flush)
        sink.file.flush();
    }
  }

  static void static_write(const std::string &msg, bool flush,
                           bool use_stderr = false) {
    get().write(msg, flush, use_stderr);
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
  std::string thread_id_str_;
  const char *thread_color_ = "";
  const char *log_color_ = "";
  const char *warn_color_ = "";
  const char *error_color_ = "";
  const char *reset_ = "";
}; // namespace myproj
} // namespace mutils
