
#include <xbot-service/portable/thread.hpp>

using namespace xbot::service::thread;

bool xbot::service::thread::initialize(ThreadPtr thread, void (*threadfunc)(void*), void* arg, void* stackbuf,
                                       size_t buflen, const char* name) {
  // Create a multicast sender thread
  *thread = chThdCreateStatic(stackbuf, buflen, NORMALPRIO, threadfunc, arg);
  (*thread)->name = name;
  return true;
}

void xbot::service::thread::deinitialize(ThreadPtr thread) {
  (void)thread;
  // Currently not implemented, since we should probably rather reset the MCU
  chDbgAssert(false, "Not Implemented");
}
