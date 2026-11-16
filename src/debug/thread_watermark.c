//
// Periodically logs the stack high-water mark (watermark) of every ChibiOS
// thread. Only available in debug builds (requires CH_DBG_FILL_THREADS, which
// fills each thread's working area with CH_DBG_STACK_FILL_VALUE on creation so
// we can scan for the deepest stack usage).
//

#include "thread_watermark.h"

#include "ch.h"
#include "chdebug.h"

#ifdef DEBUG_BUILD

#include <ulog.h>

// How often the watermark report is printed.
#define WATERMARK_INTERVAL_MS 120000

static THD_WORKING_AREA(watermark_wa, 768);

// Returns the number of stack bytes that were never touched by the thread, by
// counting the leading fill bytes from the bottom (low address) of the working
// area. used = total - free.
static size_t ThreadFreeStack(const thread_t *tp) {
  const uint8_t *base = (const uint8_t *)chThdGetWorkingAreaX((thread_t *)tp);
#if PORT_ENABLE_GUARD_PAGES == TRUE
  base += PORT_GUARD_PAGE_SIZE; /* skip the no-access guard region */
#endif
  // The thread_t structure lives at the top of the working area, so it marks
  // the end of the usable stack region.
  const uint8_t *end = (const uint8_t *)tp;
  size_t free = 0;
  while (base + free < end && base[free] == CH_DBG_STACK_FILL_VALUE) {
    free++;
  }
  return free;
}

// ChibiOS sizes a working area as context frames + thread stack + interrupt
// allowance (PORT_WA_SIZE). Only the middle part belongs to the thread, and the
// port writes its context frame directly below thread_t, so that frame shows up
// as "used" even for a thread that barely touched its stack. Subtract both to
// get the numbers the thread really has: `usable` is the size that was passed to
// THD_WORKING_AREA(), `headroom` is what is left above the measured peak.
static size_t ThreadUsableStack(size_t total) {
  size_t usable = total - PORT_WA_SIZE(0);
  return usable & ~(size_t)(PORT_STACK_ALIGN - 1U);
}

static int32_t ThreadHeadroom(size_t usable, size_t used) {
  return (int32_t)(usable + sizeof(struct port_intctx)) - (int32_t)used;
}

static THD_FUNCTION(WatermarkThread, arg) {
  (void)arg;
  chRegSetThreadName("watermark");

  chThdSleepMilliseconds(1000);

  while (true) {
    ULOG_INFO("=== Thread stack watermark ===");
    thread_t *tp = chRegFirstThread();
    while (tp != NULL) {
      const uint8_t *base = (const uint8_t *)chThdGetWorkingAreaX(tp);
#if PORT_ENABLE_GUARD_PAGES == TRUE
      base += PORT_GUARD_PAGE_SIZE; /* skip the no-access guard region */
#endif
      size_t total = (const uint8_t *)tp - base;
      size_t free = ThreadFreeStack(tp);
      size_t used = total - free;
      size_t usable = ThreadUsableStack(total);
      int32_t headroom = ThreadHeadroom(usable, used);
      const char *name = chRegGetThreadNameX(tp);
      ULOG_INFO("%-12s stack: %4u/%4u used, %5d headroom%s", name != NULL ? name : "<unnamed>", (unsigned)used,
                (unsigned)usable, (int)headroom, (headroom < 0) ? "  OVERFLOW" : "");
      tp = chRegNextThread(tp);
    }
    chThdSleepMilliseconds(WATERMARK_INTERVAL_MS);
  }
}

void InitThreadWatermark(void) {
  chThdCreateStatic(watermark_wa, sizeof(watermark_wa), LOWPRIO, WatermarkThread, NULL);
}

#else  // !DEBUG_BUILD

void InitThreadWatermark(void) {
}

#endif  // DEBUG_BUILD
