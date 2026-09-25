//
// Global newlib assert handler.
//
// Replaces newlib's default __assert_func, which formats the message for a
// stderr that goes nowhere (the libnosys _write stub always fails) and then
// runs abort() -> raise() -> the _kill/_getpid stubs - and in the process
// drags the whole newlib stdio machinery into the image (the linker warnings
// "_close/_fstat/_isatty/_lseek/_read/_write is not implemented").
//
// Instead: one CRITICAL line over the xbot remote log (UDP multicast), then
// the project's Fault_Handler policy (Release: NVIC_SystemReset, Debug:
// chSysHalt - see boards/XCORE/board.c). The handler never returns.
//
// remote_logger() is called directly instead of via ULOG_CRITICAL to bypass
// the non-recursive ulog message mutex: an assert raised while this thread is
// already inside ulog_message() must not deadlock here. remote_logger()
// serializes against concurrent loggers with its own logging_mutex, and its
// call graph contains no asserts (portable/xbot packet implementation).
//

#include <ch.h>
#include <ulog.h>

#include <cstdio>

#include "globals.hpp"

// C++ linkage on purpose: defined non-static in
// ext/xbot_framework/libxbot-service/src/RemoteLogging.cpp.
extern void remote_logger(ulog_level_t severity, char *msg, const void *args);

extern "C" void Fault_Handler(const char *reason);

extern "C" void __assert_func(const char *file, int line, const char *function, const char *expr) {
  static volatile bool in_assert = false;

  // First entry logs; re-entry (assert from inside the logging path itself)
  // and the pre-init case go straight to the terminal action. Thread context
  // is assumed - the logging path cannot run from an ISR anyway (ulog locks a
  // ChibiOS mutex), so an ISR assert still terminates via Fault_Handler.
  if (!in_assert && remote_logging_up) {
    in_assert = true;

    // %s/%d only on purpose: floating point formatting (newlib dtoa) has its
    // own internal asserts and must not be able to re-enter here. The buffer
    // is one ULOG line; truncation (long C++ function names) cuts the tail,
    // which holds the least important part ([function]).
    char msg[ULOG_MAX_MESSAGE_LENGTH];
    const int written = snprintf(msg, sizeof(msg), "assert %s:%d: %s [%s]", file, line, expr, function);
    if (written > 0) {
      remote_logger(ULOG_CRITICAL_LEVEL, msg, nullptr);
      // sendto() only queues the frame into lwIP; give the stack and the
      // Ethernet DMA a moment to put it on the wire before stopping the world.
      chThdSleepMilliseconds(20);
    }
  }

  Fault_Handler("assert");
  // Unreachable (both Fault_Handler paths are noreturn) but keeps the
  // never-returns contract explicit for the compiler.
  while (true) {
  }
}
