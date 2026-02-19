#pragma once

#include <functional>
#define DEFER_CONCAT_(a, b) a##b
#define DEFER_CONCAT(a, b) DEFER_CONCAT_(a, b)

// Usage: DEFER({ /* code to run at scope exit */ });
#define DEFER(fn)                                                              \
  mutils::ScopeGuard DEFER_CONCAT(_guard_, __LINE__)([&] { fn; })

namespace mutils {
class ScopeGuard {
public:
  explicit ScopeGuard(std::function<void()> fn) : fn_(fn) {}
  ~ScopeGuard() { fn_(); }
  ScopeGuard(const ScopeGuard &) = delete;
  ScopeGuard &operator=(const ScopeGuard &) = delete;

private:
  std::function<void()> fn_;
};

} // namespace mutils
