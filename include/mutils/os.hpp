#pragma once

#define BACKTRACE_SIZE 62

#ifdef _WIN32
#include <cstdio>
// clang-format off
#include <windows.h>
#include <dbghelp.h>
// clang-format on
#endif

#ifdef __linux__
#include <csignal>
#include <execinfo.h>
#include <unistd.h>
#endif

namespace mutils {
inline void instrumentSigsegv() {
#ifdef _WIN32
  SymInitialize(GetCurrentProcess(), nullptr, TRUE);
  AddVectoredExceptionHandler(1, [](EXCEPTION_POINTERS *ep) -> LONG WINAPI {
    printf("Crash: 0x%lx\n", ep->ExceptionRecord->ExceptionCode);

    // bss
    static void *bt_stack[BACKTRACE_SIZE];

    // On Windows, we can use the CaptureStackBackTrace function to get a
    // backtrace
    USHORT frames = CaptureStackBackTrace(0, BACKTRACE_SIZE, bt_stack, nullptr);
    HANDLE process = GetCurrentProcess();

    SYMBOL_INFO_PACKAGE sym = {};
    sym.si.SizeOfStruct = sizeof(SYMBOL_INFO);
    sym.si.MaxNameLen = MAX_SYM_NAME;

    for (USHORT i = 0; i < frames; ++i) {
      DWORD64 addr = (DWORD64)bt_stack[i];
      if (SymFromAddr(process, addr, nullptr, &sym.si)) {
        printf("Frame %d: %s (0x%llx)\n", i, sym.si.Name, addr);
      } else {
        printf("Frame %d: <unknown> (0x%llx)\n", i, addr);
      }
    }

    return EXCEPTION_EXECUTE_HANDLER;
  });
#endif
#ifdef __linux__
  // WARNING: inside a signal handler only async-signal-safe functions can be
  // called, so we must be careful to avoid any unsafe operations (e.g., heap
  // allocations, locks, etc.)
  signal(SIGSEGV, [](int signum) {});
#endif
}
} // namespace mutils
