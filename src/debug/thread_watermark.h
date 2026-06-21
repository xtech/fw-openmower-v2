//
// Periodically logs the stack high-water mark (watermark) of every ChibiOS
// thread. Only available in debug builds (requires CH_DBG_FILL_THREADS).
//

#ifndef THREAD_WATERMARK_H
#define THREAD_WATERMARK_H
#ifdef __cplusplus
extern "C" {
#endif

// Starts the background thread that periodically logs the per-thread stack
// watermark. No-op unless built with DEBUG_BUILD.
void InitThreadWatermark(void);

#ifdef __cplusplus
}
#endif
#endif  // THREAD_WATERMARK_H
