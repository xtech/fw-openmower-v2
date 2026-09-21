//
// Shared I2C master helpers: bounded transfers with retries, bus recovery and
// the per-bus statistics counters.
//
// Why this exists (learned on the Sabo charger/BMS bus on I2C1):
//   - `i2cMasterTransmit()` waits *forever*. A slave (or a bus buffer) holding a
//     line low then blocks the calling service thread permanently while it holds
//     the bus mutex, which stops everything that needs I2C. Every transfer here
//     uses a bounded timeout instead.
//   - A master must not *start* into a bus that is still held low (clock
//     stretch, unfinished byte, repeater/buffer lockup): starting anyway
//     produces ARLO/BERR bursts. Wait (bounded) for the bus to be idle first.
//   - A NACK/ARLO does not leave the peripheral in a bad state, so retry first;
//     only a software timeout (I2C_LOCKED) needs the stop/unstick/start
//     recovery.
//   - An aborted transfer can leave a slave mid-byte. 9 SCL pulses + STOP reset
//     it without touching the chip behind it - the charger used to be reset for
//     every failed read, which amplified every incident.
//
// The counters are deliberately plain numbers, so a remote log alone tells
// whether a bus is healthy (txn/fail + failure classes).
//

#ifndef XBOT_I2C_UTILS_HPP
#define XBOT_I2C_UTILS_HPP

#include <ch.h>
#include <hal.h>
#include <ulog.h>

#include <cstddef>
#include <cstdint>

#include "board.h"

namespace xbot::i2c {

// ---------------------------------------------------------------------------
// Policy
// ---------------------------------------------------------------------------

// Bounded timeout for a single transfer. One byte (incl. ACK) takes ~90 us at
// 100 kHz, so 20 ms is very generous - but it is what keeps a held bus from
// blocking a service thread forever.
constexpr uint32_t kTransferTimeoutMs = 20;

// Attempts per transfer before the peripheral is stopped and recovered.
constexpr unsigned kAttemptsPerTransfer = 3;

// Delay between attempts and before a recovery.
constexpr uint32_t kRetryDelayMs = 2;

// Budget for waiting until both lines are high before a transfer is started,
// counted in steps of one millisecond - the shortest delay the kernel can
// produce here (tick-less mode, CH_CFG_ST_TIMEDELTA = 10 ticks = 1 ms), asking
// for microseconds is clamped to that. The wait only happens when the bus is
// actually not idle, a healthy bus returns immediately.
constexpr uint32_t kBusIdleWaitSteps = 3;

// Restart attempts (`i2cStart`) during a recovery before giving up.
constexpr unsigned kRestartAttempts = 3;

// Active unstick: 9 SCL pulses + STOP, driven as GPIO open drain.
// The half period is one millisecond, the shortest delay the kernel can produce
// here (tick-less mode, CH_CFG_ST_TIMEDELTA = 10 ticks = 1 ms) - asking for
// microseconds is clamped to that. A burst is therefore ~25 ms and a full
// recovery ~28 ms, which is fine: it runs once per failed transfer and a stuck
// slave does not care about the clock rate. (chSysPolledDelayX() would give true
// microseconds if a fast recovery were ever needed.)
constexpr unsigned kUnstickPulses = 9;
constexpr uint32_t kUnstickHalfPeriodMs = 1;

// Consecutive failed charger polls (1 Hz) tolerated before PowerService resets the
// charger IC and rewrites all of its registers. A single flaky read (one NACK
// happens even on a healthy bus) must not trigger a chip reset - the reset adds
// bus traffic and clears the charger's fault status, which hides why it happened.
constexpr uint32_t kChargerFailTicksBeforeReinit = 3;

// How long to stay off the bus after a recovery that did not help (the bus
// stayed held low, or the peripheral could not be restarted). Without it every
// transfer of every tick re-runs the full recovery and logs four lines, which
// makes a field log unreadable and keeps the bus mutex busy for ~30 ms per
// transfer; with it one recovery runs per interval and the rest fail fast.
constexpr uint32_t kBusDownRetryMs = 1000;

// Interval of the per-bus status line. Mirrors the thread watermark cadence
// (WATERMARK_INTERVAL_MS in src/debug/thread_watermark.c, not exported via a
// header) so the log has one steady rhythm instead of two.
constexpr uint32_t kStatsIntervalMs = 120000;

// How often failed attempts are logged: every failure up to kFailLogAll, after
// that every kFailLogEvery-th one. The counters always count all of them.
constexpr uint32_t kFailLogAll = 10;
constexpr uint32_t kFailLogEvery = 100;

// ---------------------------------------------------------------------------
// Per-bus statistics
// ---------------------------------------------------------------------------

// The field names are the tokens used in the status line (they are meant to be
// read in a field log), so the two cannot drift apart.
struct Stats {
  uint32_t txn = 0;          // transfers attempted (including retries)
  uint32_t retry = 0;        // attempts that were a retry
  uint32_t fail = 0;         // transfers that failed on all attempts (=> recovered)
  uint32_t recover = 0;      // bus recoveries (stop + unstick + start)
  uint32_t restartfail = 0;  // of those: the peripheral could not be restarted
  uint32_t busheld = 0;      // of those: a line was measurably held low
  uint32_t skipped = 0;      // transfers not even started (bus stayed held low)
  // Failure classes of the failed attempts (a transfer can add several)
  uint32_t nack = 0;        // slave did not acknowledge
  uint32_t berr = 0;        // bus error
  uint32_t arlo = 0;        // arbitration lost
  uint32_t ovr = 0;         // overrun
  uint32_t hw_timeout = 0;  // peripheral timeout flag (I2C_TIMEOUT)
  uint32_t sw_timeout = 0;  // transfer returned MSG_TIMEOUT
  uint32_t other = 0;       // failed without an error flag
  // Settled line levels of the last transfer start (1 = high), printed as `SCL/SDA=`.
  uint8_t last_scl = 1;
  uint8_t last_sda = 1;
  uint32_t last_log_ms = 0;
  // While chVTGetSystemTimeX() is before this, the bus is known-broken (a
  // recovery did not help, see kBusDownRetryMs) and transfers fail fast.
  uint32_t retry_at_ms = 0;
};

constexpr size_t kBusCount = 4U;

// One stats block per bus. Namespace scope instead of function-local statics,
// because the firmware is built with -fno-threadsafe-statics. Only touched from
// thread context while the caller holds the bus mutex, so no extra locking.
inline Stats s_stats[kBusCount];

inline size_t BusIndex(const I2CDriver* drv) {
#if STM32_I2C_USE_I2C1
  if (drv == &I2CD1) return 0U;
#endif
#if STM32_I2C_USE_I2C2
  if (drv == &I2CD2) return 1U;
#endif
#if STM32_I2C_USE_I2C3
  if (drv == &I2CD3) return 2U;
#endif
#if STM32_I2C_USE_I2C4
  if (drv == &I2CD4) return 3U;
#endif
  return 0U;
}

inline const char* BusName(size_t idx) {
  static const char* const kNames[kBusCount] = {"I2C1", "I2C2", "I2C3", "I2C4"};
  return (idx < kBusCount) ? kNames[idx] : "I2C?";
}

inline Stats& StatsFor(const I2CDriver* drv) {
  const size_t idx = BusIndex(drv);
  return s_stats[idx < kBusCount ? idx : 0U];
}

// Short name of the (dominant) error class in an i2cflags_t mask.
inline const char* ErrorName(i2cflags_t errors) {
  if ((errors & I2C_ACK_FAILURE) != 0U) return "NACK";
  if ((errors & I2C_BUS_ERROR) != 0U) return "BERR";
  if ((errors & I2C_ARBITRATION_LOST) != 0U) return "ARLO";
  if ((errors & I2C_OVERRUN) != 0U) return "OVR";
  if ((errors & I2C_TIMEOUT) != 0U) return "HWTO";
  if (errors != I2C_NO_ERROR) return "ERR";
  return "OK";
}

// ---------------------------------------------------------------------------
// Bus lines, unstick and recovery
// ---------------------------------------------------------------------------

struct Lines {
  ioline_t scl;
  ioline_t sda;
};

inline Lines LinesFor(size_t idx) {
  switch (idx) {
    case 0: return {LINE_I2C1_SCL, LINE_I2C1_SDA};
#if defined(LINE_I2C2_SCL)
    case 1: return {LINE_I2C2_SCL, LINE_I2C2_SDA};
#endif
#if defined(LINE_I2C3_SCL)
    case 2: return {LINE_I2C3_SCL, LINE_I2C3_SDA};
#endif
#if defined(LINE_I2C4_SCL)
    case 3: return {LINE_I2C4_SCL, LINE_I2C4_SDA};
#endif
    default: return {PAL_NOLINE, PAL_NOLINE};
  }
}

// Rebuilds the pin mode the I2C peripheral needs (alternate function + open
// drain) for one line by *reading it back* from the GPIO pinmux.
//
// Open drain is what every I2C pin uses, so it does not have to be read back
// along with the AF number. (ChibiOS 21.11 has no palGetLineMode(), otherwise
// that would be the natural choice. PAL_PORT()/PAL_PAD() are portable ChibiOS
// macros, the AFRL/AFRH layout is STM32 specific - like the rest of this file.)
inline iomode_t ReadI2cPinMode(ioline_t line) {
  const ioportid_t port = PAL_PORT(line);
  const iopadid_t pad = PAL_PAD(line);
  // This port's register block exposes the AF selection as AFRL (pads 0..7) and
  // AFRH (pads 8..15); the nibble for pad n lives at bit (n & 7) * 4.
  const uint32_t afr = (pad < 8U) ? port->AFRL : port->AFRH;
  const uint32_t af = (afr >> ((pad & 7U) * 4U)) & 0xFU;
  return PAL_STM32_MODE_ALTERNATE | PAL_STM32_OTYPE_OPENDRAIN | PAL_STM32_ALTERNATE(af);
}

// The pin modes of both lines of a bus (see ReadI2cPinMode).
struct PinModes {
  iomode_t scl;
  iomode_t sda;
};

// Reads back the pin modes the board configured for both lines of a bus. Must be
// called while the pins are still in their alternate-function mode, i.e. before
// the unstick drives them as plain GPIO.
inline PinModes ReadI2cPinModes(const Lines& lines) {
  if ((lines.scl == PAL_NOLINE) || (lines.sda == PAL_NOLINE)) {
    return PinModes{PAL_MODE_INPUT, PAL_MODE_INPUT};
  }
  return PinModes{ReadI2cPinMode(lines.scl), ReadI2cPinMode(lines.sda)};
}

// Reads the SCL/SDA line levels (0 = low, 1 = high). Works with the pins in
// their alternate function mode as well as in input mode.
inline void ReadBusLines(size_t idx, uint8_t* scl, uint8_t* sda) {
  const Lines lines = LinesFor(idx);
  if (scl != nullptr) {
    *scl = (lines.scl != PAL_NOLINE) ? (palReadLine(lines.scl) ? 1U : 0U) : 0U;
  }
  if (sda != nullptr) {
    *sda = (lines.sda != PAL_NOLINE) ? (palReadLine(lines.sda) ? 1U : 0U) : 0U;
  }
}

inline void ReadBusLines(const I2CDriver* drv, uint8_t* scl, uint8_t* sda) {
  ReadBusLines(BusIndex(drv), scl, sda);
}

// Puts the bus pins into plain input mode with pull-up: nothing of ours drives
// them any more, so the *external* bus state can be measured reliably.
inline void SetPinsPassive(size_t idx) {
  const Lines lines = LinesFor(idx);
  if ((lines.scl == PAL_NOLINE) || (lines.sda == PAL_NOLINE)) {
    return;
  }
  palSetLineMode(lines.scl, PAL_MODE_INPUT_PULLUP);
  palSetLineMode(lines.sda, PAL_MODE_INPUT_PULLUP);
  chThdSleepMilliseconds(1);  // let the lines settle (>= the tick-less minimum)
}

// Hands the pins back to the I2C peripheral (must happen before i2cStart()),
// using the modes that were read back before they were driven as GPIO.
inline void SetPinsI2c(const Lines& lines, const PinModes& modes) {
  if ((lines.scl == PAL_NOLINE) || (lines.sda == PAL_NOLINE)) {
    return;
  }
  palSetLineMode(lines.scl, modes.scl);
  palSetLineMode(lines.sda, modes.sda);
}

// Drives SCL as a GPIO open drain for 9 pulses (which lets a confused slave
// finish its byte and release SDA) and then generates a STOP. Also cleans up a
// slave that was left mid-byte with its lines already released - the next
// address would otherwise collide with what it is still clocking out.
// The caller must have put the pins into a passive/GPIO state first
// (SetPinsPassive) and hands them back to the peripheral afterwards. Leaves the
// pins passive (input + pull-up) so the caller can sample the true bus state.
inline void UnstickBus(size_t idx) {
  const Lines lines = LinesFor(idx);
  if ((lines.scl == PAL_NOLINE) || (lines.sda == PAL_NOLINE)) {
    return;
  }

  palSetLineMode(lines.scl, PAL_MODE_OUTPUT_OPENDRAIN);
  palSetLineMode(lines.sda, PAL_MODE_OUTPUT_OPENDRAIN);
  palSetLine(lines.sda);  // release SDA
  for (unsigned i = 0; i < kUnstickPulses; i++) {
    palClearLine(lines.scl);
    chThdSleepMilliseconds(kUnstickHalfPeriodMs);
    palSetLine(lines.scl);
    chThdSleepMilliseconds(kUnstickHalfPeriodMs);
  }
  // STOP condition: SDA goes low then high while SCL stays high.
  palClearLine(lines.sda);
  chThdSleepMilliseconds(kUnstickHalfPeriodMs);
  palSetLine(lines.scl);
  chThdSleepMilliseconds(kUnstickHalfPeriodMs);
  palSetLine(lines.sda);
  chThdSleepMilliseconds(kUnstickHalfPeriodMs);
  // Leave the pins passive (input + pull-up) so the caller can sample the true
  // bus state before handing them back to the peripheral.
  palSetLineMode(lines.scl, PAL_MODE_INPUT_PULLUP);
  palSetLineMode(lines.sda, PAL_MODE_INPUT_PULLUP);
  chThdSleepMilliseconds(1);  // let the lines settle (>= the tick-less minimum)
}

// Stops the peripheral (so it releases the pins and cannot fight the GPIO
// pulses), unsticks the bus and restarts the peripheral.
inline void RecoverBus(I2CDriver* i2c, const char* tag) {
  Stats& s = StatsFor(i2c);
  const size_t idx = BusIndex(i2c);
  const Lines lines = LinesFor(idx);

  // Read back the pin modes the board configured for this bus *before* they get
  // driven as plain GPIO (see ReadI2cPinMode), so they can be handed back
  // unchanged afterwards. The pins are in their alternate-function mode here.
  const PinModes pin_modes = ReadI2cPinModes(lines);

  // Give the slave a moment to finish its internal state machine, then stop the
  // peripheral: while it is enabled it keeps driving the pins and would fight
  // the GPIO bit-banging (and it may itself hold SCL low mid-transfer).
  const auto old_cfg = i2c->config;
  chThdSleepMilliseconds(kRetryDelayMs);
  i2cStop(i2c);

  uint8_t before_scl = 1;
  uint8_t before_sda = 1;
  SetPinsPassive(idx);
  ReadBusLines(idx, &before_scl, &before_sda);
  const bool was_held = (before_scl == 0U) || (before_sda == 0U);

  UnstickBus(idx);

  uint8_t after_scl = 1;
  uint8_t after_sda = 1;
  ReadBusLines(idx, &after_scl, &after_sda);  // the pins are still passive here
  SetPinsI2c(lines, pin_modes);

  s.recover++;
  if (was_held) {
    s.busheld++;
    ULOG_WARNING("%s %s: bus was held low (SCL/SDA=%u/%u) - unsticked with 9 clocks + STOP (now SCL/SDA=%u/%u)", tag,
                 BusName(idx), (unsigned)before_scl, (unsigned)before_sda, (unsigned)after_scl, (unsigned)after_sda);
    if ((after_scl == 0U) || (after_sda == 0U)) {
      ULOG_ERROR("%s %s: bus is still held low after the unstick (SCL/SDA=%u/%u)", tag, BusName(idx),
                 (unsigned)after_scl, (unsigned)after_sda);
    }
  }

  // Restart the peripheral. Bounded on purpose: a bus that never comes back
  // must not spin here forever (the caller continues and reports the failure).
  for (unsigned attempt = 0; attempt < kRestartAttempts; attempt++) {
    if (i2cStart(i2c, old_cfg) == HAL_RET_SUCCESS) {
      return;
    }
    chThdSleepMilliseconds(kRetryDelayMs);
  }
  s.restartfail++;
  s.retry_at_ms = (uint32_t)TIME_I2MS(chVTGetSystemTimeX()) + kBusDownRetryMs;
  ULOG_ERROR("%s %s: I2C restart failed %u times - peripheral stays disabled", tag, BusName(idx),
             (unsigned)kRestartAttempts);
}

// Waits (bounded) until both lines are high. Returns false if the bus stayed
// held low, in which case the caller must recover instead of starting into it.
// The budget is counted in polling steps (see kBusIdleWaitSteps).
inline bool WaitForBusIdle(const I2CDriver* drv, uint32_t steps) {
  uint8_t scl = 1;
  uint8_t sda = 1;
  ReadBusLines(drv, &scl, &sda);
  for (uint32_t i = 0; ((scl == 0U) || (sda == 0U)) && (i < steps); i++) {
    chThdSleepMilliseconds(1);  // the shortest step the kernel can do (see above)
    ReadBusLines(drv, &scl, &sda);
  }
  if ((scl == 0U) || (sda == 0U)) {
    return false;  // still held low
  }
  Stats& s = StatsFor(drv);
  s.last_scl = scl;
  s.last_sda = sda;
  return true;
}

// ---------------------------------------------------------------------------
// Statistics / logging
// ---------------------------------------------------------------------------

// Logs the per-bus counters (rate limited, see kStatsIntervalMs).
inline void LogStatsIfDue(const I2CDriver* drv) {
  Stats& s = StatsFor(drv);
  const uint32_t now = (uint32_t)TIME_I2MS(chVTGetSystemTimeX());
  if ((s.last_log_ms != 0U) && ((now - s.last_log_ms) < kStatsIntervalMs)) {
    return;
  }
  s.last_log_ms = now;
  if ((s.txn == 0U) && (s.fail == 0U)) {
    return;  // this bus is not used by anyone
  }

  const char* bus_name = BusName(BusIndex(drv));
  ULOG_INFO("%s txn=%u fail=%u retry=%u recover=%u restartfail=%u busheld=%u skipped=%u SCL/SDA=%u/%u", bus_name,
            (unsigned)s.txn, (unsigned)s.fail, (unsigned)s.retry, (unsigned)s.recover, (unsigned)s.restartfail,
            (unsigned)s.busheld, (unsigned)s.skipped, (unsigned)s.last_scl, (unsigned)s.last_sda);

  if ((s.nack + s.berr + s.arlo + s.ovr + s.hw_timeout + s.sw_timeout + s.other) > 0U) {
    ULOG_INFO("%s errors nack=%u berr=%u arlo=%u ovr=%u hw_timeout=%u sw_timeout=%u other=%u", bus_name,
              (unsigned)s.nack, (unsigned)s.berr, (unsigned)s.arlo, (unsigned)s.ovr, (unsigned)s.hw_timeout,
              (unsigned)s.sw_timeout, (unsigned)s.other);
  }
}

// Counts a failed transfer attempt and logs it (rate limited; the counters
// always count every attempt).
inline void LogFailedAttempt(I2CDriver* i2c, const char* tag, uint8_t addr, const uint8_t* tx, size_t tx_len,
                             size_t rx_len, msg_t msg, i2cflags_t errs) {
  Stats& s = StatsFor(i2c);
  if ((errs & I2C_ACK_FAILURE) != 0U) s.nack++;
  if ((errs & I2C_BUS_ERROR) != 0U) s.berr++;
  if ((errs & I2C_ARBITRATION_LOST) != 0U) s.arlo++;
  if ((errs & I2C_OVERRUN) != 0U) s.ovr++;
  if ((errs & I2C_TIMEOUT) != 0U) s.hw_timeout++;
  if (msg == MSG_TIMEOUT) s.sw_timeout++;
  if ((errs == I2C_NO_ERROR) && (msg != MSG_TIMEOUT)) s.other++;

  const uint32_t failures = s.nack + s.berr + s.arlo + s.ovr + s.hw_timeout + s.sw_timeout + s.other;
  if ((failures > kFailLogAll) && ((failures % kFailLogEvery) != 0U)) {
    return;
  }

  uint8_t scl = 0;
  uint8_t sda = 0;
  ReadBusLines(i2c, &scl, &sda);
  ULOG_WARNING("%s %s failed addr=0x%02X reg=0x%02X %s len=%u/%u msg=%d errs=0x%02X (%s) SCL/SDA=%u/%u", tag,
               BusName(BusIndex(i2c)), (unsigned)addr, ((tx_len > 0U) && (tx != nullptr)) ? (unsigned)tx[0] : 0xFFU,
               (rx_len > 0U) ? "read" : "write", (unsigned)tx_len, (unsigned)rx_len, (int)msg, (unsigned)errs,
               ErrorName(errs), (unsigned)scl, (unsigned)sda);
}

// ---------------------------------------------------------------------------
// Transfer helper
// ---------------------------------------------------------------------------

// One transfer attempt, used by the wrapper below (i2cMasterTransmitTimeout
// covers a pure write, a write+read with a repeated START and a pure read).
inline msg_t TransferOnce(I2CDriver* i2c, uint8_t addr, const uint8_t* tx, size_t tx_len, uint8_t* rx, size_t rx_len) {
  return i2cMasterTransmitTimeout(i2c, addr, tx, tx_len, rx, rx_len, TIME_MS2I(kTransferTimeoutMs));
}

// Wraps i2cMasterTransmitTimeout with the bounded timeout, the retries, the
// statistics and the bus recovery described at the top of this file.
inline msg_t TransmitWithRecovery(I2CDriver* i2c, uint8_t addr, const uint8_t* tx, size_t tx_len, uint8_t* rx,
                                  size_t rx_len, const char* tag) {
  if (i2c == nullptr) {
    return MSG_RESET;
  }

  Stats& s = StatsFor(i2c);
  const uint32_t now = (uint32_t)TIME_I2MS(chVTGetSystemTimeX());

  // Backoff: the bus is known-broken (a recovery did not help, see
  // kBusDownRetryMs). Fail fast instead of running the recovery cycle and its
  // log lines for every transfer. The first attempt after that runs the full
  // sequence again. The comparison is wrap-safe (signed difference).
  if ((s.retry_at_ms != 0U) && ((int32_t)(now - s.retry_at_ms) < 0)) {
    s.skipped++;
    return MSG_TIMEOUT;
  }
  s.retry_at_ms = 0U;

  // Never start into a bus that is still held low.
  if (!WaitForBusIdle(i2c, kBusIdleWaitSteps)) {
    ULOG_WARNING("%s bus is not idle - recovering before the transfer", tag);
    RecoverBus(i2c, tag);
    if (!WaitForBusIdle(i2c, kBusIdleWaitSteps)) {
      s.skipped++;
      s.retry_at_ms = now + kBusDownRetryMs;
      LogStatsIfDue(i2c);
      ULOG_ERROR("%s bus is still held low - transfer skipped, next attempt in %u ms", tag, (unsigned)kBusDownRetryMs);
      return MSG_TIMEOUT;
    }
  }

  msg_t msg = MSG_TIMEOUT;
  for (unsigned attempt = 0; attempt < kAttemptsPerTransfer; attempt++) {
    if (attempt > 0U) {
      s.retry++;
    }
    s.txn++;

    msg = TransferOnce(i2c, addr, tx, tx_len, rx, rx_len);
    if (msg == MSG_OK) {
      LogStatsIfDue(i2c);
      return msg;
    }

    const i2cflags_t errs = i2cGetErrors(i2c);
    LogFailedAttempt(i2c, tag, addr, tx, tx_len, rx_len, msg, errs);
    LogStatsIfDue(i2c);

    // Only a software timeout (the peripheral is left I2C_LOCKED) requires a
    // restart, and a retry is far less invasive than a recovery - retry first.
    const bool locked = (i2c->state == I2C_LOCKED);
    if (locked || (msg == MSG_TIMEOUT) || ((attempt + 1U) >= kAttemptsPerTransfer)) {
      break;
    }
    chThdSleepMilliseconds(kRetryDelayMs);
  }

  s.fail++;
  RecoverBus(i2c, tag);
  LogStatsIfDue(i2c);
  return msg;
}

}  // namespace xbot::i2c

#endif  // XBOT_I2C_UTILS_HPP
