// ulog_rate_limit.hpp (C/C++ compatible)
#pragma once

#include "ch.h"
#include "ulog.h"

// Helpers to make per-callsite unique statics
#define ULOG_RL_JOIN_INNER(a, b) a##b
#define ULOG_RL_JOIN(a, b) ULOG_RL_JOIN_INNER(a, b)

// Log at most once per period_ms
#define ULOG_EVERY_MS(level, period_ms, fmt, ...)                                          \
  do {                                                                                     \
    static systime_t ULOG_RL_JOIN(_ulog_last_, __LINE__) = 0;                              \
    systime_t _now = chVTGetSystemTimeX();                                                 \
    if (ULOG_RL_JOIN(_ulog_last_, __LINE__) == 0 ||                                        \
        (systime_t)(_now - ULOG_RL_JOIN(_ulog_last_, __LINE__)) >= TIME_MS2I(period_ms)) { \
      ULOG_RL_JOIN(_ulog_last_, __LINE__) = _now;                                          \
      ULOG_##level(fmt, ##__VA_ARGS__);                                                    \
    }                                                                                      \
  } while (0)

// Log only once per callsite
#define ULOG_ONCE(level, fmt, ...)                      \
  do {                                                  \
    static int ULOG_RL_JOIN(_ulog_once_, __LINE__) = 0; \
    if (!ULOG_RL_JOIN(_ulog_once_, __LINE__)) {         \
      ULOG_RL_JOIN(_ulog_once_, __LINE__) = 1;          \
      ULOG_##level(fmt, ##__VA_ARGS__);                 \
    }                                                   \
  } while (0)

// Log every Nth occurrence
#define ULOG_EVERY_N(level, n, fmt, ...)                               \
  do {                                                                 \
    static unsigned ULOG_RL_JOIN(_ulog_cnt_, __LINE__) = 0;            \
    if ((++ULOG_RL_JOIN(_ulog_cnt_, __LINE__)) % (unsigned)(n) == 0) { \
      ULOG_##level(fmt, ##__VA_ARGS__);                                \
    }                                                                  \
  } while (0)

// Arg variants (pass a tag/context via uLog arg)
#define ULOG_ARG_EVERY_MS(level, period_ms, arg, fmt, ...)                                     \
  do {                                                                                         \
    static systime_t ULOG_RL_JOIN(_ulog_last_arg_, __LINE__) = 0;                              \
    systime_t _now = chVTGetSystemTimeX();                                                     \
    if (ULOG_RL_JOIN(_ulog_last_arg_, __LINE__) == 0 ||                                        \
        (systime_t)(_now - ULOG_RL_JOIN(_ulog_last_arg_, __LINE__)) >= TIME_MS2I(period_ms)) { \
      ULOG_RL_JOIN(_ulog_last_arg_, __LINE__) = _now;                                          \
      ULOG_ARG_##level(arg, fmt, ##__VA_ARGS__);                                               \
    }                                                                                          \
  } while (0)

// Optional: convenience wrappers if a string tag is provided by the file/class
#ifdef LOG_TAG_STR

// Prefix formatting helper
#define _ULOG_PREFIX_FMT(fmt) "[%s] " fmt

// Tagged, severity-parameterized wrappers (pass severity token: TRACE, DEBUG, INFO, ...)
#define ULOGT(level, fmt, ...) ULOG_##level(_ULOG_PREFIX_FMT(fmt), LOG_TAG_STR, ##__VA_ARGS__)

#define ULOGT_EVERY_MS(level, period_ms, fmt, ...) \
  ULOG_EVERY_MS(level, period_ms, _ULOG_PREFIX_FMT(fmt), LOG_TAG_STR, ##__VA_ARGS__)

#define ULOGT_ONCE(level, fmt, ...) ULOG_ONCE(level, _ULOG_PREFIX_FMT(fmt), LOG_TAG_STR, ##__VA_ARGS__)

#define ULOGT_EVERY_N(level, n, fmt, ...) ULOG_EVERY_N(level, n, _ULOG_PREFIX_FMT(fmt), LOG_TAG_STR, ##__VA_ARGS__)

#endif  // LOG_TAG_STR
