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

static THD_WORKING_AREA(watermark_wa, 512);

// Returns the number of stack bytes that were never touched by the thread, by
// counting the leading fill bytes from the bottom (low address) of the working
// area. used = total - free.
static size_t ThreadFreeStack(const thread_t *tp) {
  const uint8_t *base = (const uint8_t *)chThdGetWorkingAreaX((thread_t *)tp);
  // The thread_t structure lives at the top of the working area, so it marks
  // the end of the usable stack region.
  const uint8_t *end = (const uint8_t *)tp;
  size_t free = 0;
  while (base + free < end && base[free] == CH_DBG_STACK_FILL_VALUE) {
    free++;
  }
  return free;
}

static THD_FUNCTION(WatermarkThread, arg) {
  (void)arg;
  chRegSetThreadName("watermark");

  while (true) {
    ULOG_INFO("=== Thread stack watermark ===");
    thread_t *tp = chRegFirstThread();
    while (tp != NULL) {
      const uint8_t *base = (const uint8_t *)chThdGetWorkingAreaX(tp);
      size_t total = (const uint8_t *)tp - base;
      size_t free = ThreadFreeStack(tp);
      const char *name = chRegGetThreadNameX(tp);
      ULOG_INFO("%-12s stack: %4u/%4u used, %4u free", name != NULL ? name : "<unnamed>", (unsigned)(total - free),
                (unsigned)total, (unsigned)free);
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
