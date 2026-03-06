#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <source_location>
#include <sstream>
#include <string_view>
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

inline bool tty = ISATTY(FILENO(stdout)) || ISATTY(FILENO(stderr));

#ifndef LOG
#define LOG mutils::Logger::get().log
#endif

#ifndef LOG_WCTX
#define LOG_WCTX(...)                                                          \
  mutils::Logger::get().log_wctx(std::source_location::current(), __VA_ARGS__)
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
struct StaticConfig {
  const std::string_view log_color;
  const std::string_view warn_color;
  const std::string_view error_color;
  const std::string_view reset;
  const bool is_tty = false;

  static const StaticConfig &get() {
    static StaticConfig cfg = [] {
      return StaticConfig{
          tty ? "\033[32m" : "",
          tty ? "\033[33m" : "",
          tty ? "\033[31m" : "",
          tty ? "\033[0m" : "",
          tty,
      };
    }();
    return cfg;
  }
};

constexpr std::string_view extract_context(std::string_view fn,
                                           int level) noexcept {
  if (fn.empty())
    return {};

  // strip nothing at the highest log level
  if (level == 4)
    return fn;

  // --- strip parameters: everything from the last '(' onward ---
  // We want the last '(' that belongs to the parameter list, not a lambda's
  // operator() argument list, so take the first '(' for simplicity —
  // works for the common case of regular methods/functions.
  auto paren = fn.find('(');
  if (paren != std::string_view::npos)
    fn = fn.substr(0, paren);

  // --- strip return type / leading keywords ---
  // The qualified name starts after the last space (if any).
  auto space = fn.rfind(' ');
  if (space != std::string_view::npos)
    fn = fn.substr(space + 1);

  // fn is now something like "dwarf::Renderer::draw" or "free_func"

  // --- strip the final ::member to get the owning scope ---
  auto last_colon = fn.rfind("::");
  if (last_colon == std::string_view::npos)
    return {}; // free function — no context

  return fn.substr(0, last_colon);
}

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

enum class LogLevel { DEBUG, INFO, WARN, ERR };

class Logger {
public:
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;
  Logger(Logger &&) = delete;
  Logger &operator=(Logger &&) = delete;

  static Logger &get() {
    thread_local Logger instance{};
    return instance;
  }

  static bool init_file(const std::filesystem::path &path,
                        bool append = false) {
    return LogSink::get().open(path, append);
  }

  static void close_file() { LogSink::get().close(); }

  template <typename... Args>
  inline void log_wctx(std::source_location loc,
                       std::format_string<Args...> fmt,
                       Args &&...fmt_args) const {
    log_impl_(LogLevel::INFO, /*flush=*/false, /*use_stderr=*/false, loc, fmt,
              std::forward<Args>(fmt_args)...);
  }

  template <typename... Args>
  inline void log(std::format_string<Args...> fmt, Args &&...fmt_args) const {
    log_impl_(LogLevel::INFO, /*flush=*/false, /*use_stderr=*/false, fmt,
              std::forward<Args>(fmt_args)...);
  }

  template <typename... Args>
  inline void dbg(std::format_string<Args...> fmt, Args &&...fmt_args) const {
    if constexpr (IS_DEBUG_BUILD) {
      log_impl_(LogLevel::DEBUG, /*flush=*/false, /*use_stderr=*/false, fmt,
                std::forward<Args>(fmt_args)...);
    }
  }

  template <typename... Args>
  inline void err(std::format_string<Args...> fmt, Args &&...fmt_args) const {
    log_impl_(LogLevel::ERR, /*flush=*/false, /*use_stderr=*/true, fmt,
              std::forward<Args>(fmt_args)...);
  }

  template <typename... Args>
  inline void warn(std::format_string<Args...> fmt, Args &&...fmt_args) const {
    log_impl_(LogLevel::WARN, /*flush=*/false, /*use_stderr=*/true, fmt,
              std::forward<Args>(fmt_args)...);
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

private:
  Logger() : thread_id_(std::this_thread::get_id()) {
    std::ostringstream oss;
    oss << thread_id_;
    std::string thread_id_str_ = oss.str();

    auto thread_color = compute_thread_color();
    auto base = thread_prefix_buf.data();
    size_t offset = 0;

    if (config_.is_tty) {
      std::memcpy(base, thread_color.data(), thread_color.size());
      offset += thread_color.size();
    }

    std::string_view thread_label = "[THREAD ";
    std::memcpy(base + offset, thread_label.data(), thread_label.size());
    offset += thread_label.size();

    std::memcpy(base + offset, thread_id_str_.data(), thread_id_str_.size());
    offset += thread_id_str_.size();

    std::string_view end = "] ";
    std::memcpy(base + offset, end.data(), end.size());
    offset += end.size();

    thread_prefix_ = std::string_view(base, offset);
  }

  void write_thread_() const {
    std::memcpy(buf_.data() + buf_offset_, thread_prefix_.data(),
                thread_prefix_.size());
    buf_offset_ += thread_prefix_.size();
  }

  void write_level_(const LogLevel level) const {
    std::string_view color, label;

    switch (level) {
    case LogLevel::DEBUG:
      color = config_.log_color;
      label = debug_label_;
      break;
    case LogLevel::INFO:
      color = config_.log_color;
      label = log_label_;
      break;
    case LogLevel::WARN:
      color = config_.warn_color;
      label = warn_label_;
      break;
    case LogLevel::ERR:
      color = config_.error_color;
      label = err_label_;
      break;
    }

    std::memcpy(buf_.data() + buf_offset_, color.data(), color.size());
    buf_offset_ += color.size();

    std::memcpy(buf_.data() + buf_offset_, label.data(), label.size());
    buf_offset_ += label.size();
  }

  void write_context_tag(const std::source_location &loc) const {
    auto ctx = extract_context(loc.function_name(), 4);
    if (ctx.empty())
      return;

    memcpy(buf_.data() + buf_offset_, "[", 1);
    buf_offset_ += 1;
    memcpy(buf_.data() + buf_offset_, ctx.data(), ctx.size());
    buf_offset_ += ctx.size();
    memcpy(buf_.data() + buf_offset_, "] ", 2);
    buf_offset_ += 2;
  }

  void write(const std::string_view msg, bool flush,
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

  template <typename... Args>
  inline void log_impl_(LogLevel level, bool flush, bool use_stderr,
                        std::format_string<Args...> fmt, Args &&...args) const {
    buf_offset_ = 0;
    write_thread_();
    write_level_(level);

    auto res =
        std::format_to_n(buf_.data() + buf_offset_, buf_.size() - buf_offset_,
                         fmt, std::forward<Args>(args)...);

    ptrdiff_t remaining_space =
        buf_.size() - (buf_offset_ + res.size + 1 + config_.reset.size());

    if (remaining_space < 0) {
      auto body = std::format(fmt, std::forward<Args>(args)...);
      auto msg = std::string(std::string_view(buf_.data(), buf_offset_)) +
                 body + std::string(config_.reset);
      write(msg, /*flush=*/false);
      return;
    }

    char *end = res.out;

    std::memcpy(end, config_.reset.data(), config_.reset.size());
    end += config_.reset.size();
    *end = '\0';

    write(std::string_view{buf_.data(), end}, flush, use_stderr);
  }

  // log with location overload
  template <typename... Args>
  inline void log_impl_(LogLevel level, bool flush, bool use_stderr,
                        const std::source_location &loc,
                        std::format_string<Args...> fmt, Args &&...args) const {
    buf_offset_ = 0;
    write_thread_();
    write_context_tag(loc);
    write_level_(level);

    auto res =
        std::format_to_n(buf_.data() + buf_offset_, buf_.size() - buf_offset_,
                         fmt, std::forward<Args>(args)...);

    ptrdiff_t remaining_space =
        buf_.size() - (buf_offset_ + res.size + 1 + config_.reset.size());

    if (remaining_space < 0) {
      auto body = std::format(fmt, std::forward<Args>(args)...);
      auto msg = std::string(std::string_view(buf_.data(), buf_offset_)) +
                 body + std::string(config_.reset);
      write(msg, /*flush=*/false);
      return;
    }

    char *end = res.out;

    std::memcpy(end, config_.reset.data(), config_.reset.size());
    end += config_.reset.size();
    *end = '\0';

    write(std::string_view{buf_.data(), end}, flush, use_stderr);
  }

  static void static_write(const std::string_view msg, bool flush,
                           bool use_stderr = false) {
    get().write(msg, flush, use_stderr);
  }

  const std::string_view compute_thread_color() const {
    size_t hash = std::hash<std::thread::id>{}(thread_id_);

    static constexpr std::string_view colors[] = {
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

  mutable std::array<char, 512> buf_;
  mutable size_t buf_offset_ = 0;
  std::thread::id thread_id_;
  std::array<char, 128> thread_prefix_buf;
  std::string_view thread_prefix_;
  std::string_view log_label_ = "[LOG]: ";
  std::string_view err_label_ = "[ERROR]: ";
  std::string_view warn_label_ = "[WARN]: ";
  std::string_view debug_label_ = "[DEBUG]: ";
  const StaticConfig &config_ = StaticConfig::get();
}; // namespace myproj
} // namespace mutils
