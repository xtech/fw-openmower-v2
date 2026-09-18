//
// Shared I2C helpers.
//

#ifndef XBOT_I2C_UTILS_HPP
#define XBOT_I2C_UTILS_HPP

#include <ch.h>
#include <hal.h>
#include <ulog.h>

#include <cstddef>

namespace xbot::i2c {

// Restarts a peripheral left unusable by a failed transfer. A bus timeout
// leaves it in the I2C_LOCKED state and it must be restarted (i2cStop +
// i2cStart) before it can be used again; this loops until the restart
// succeeds. The caller must already hold the I2C bus.
//
// `tag` prefixes the log lines so callers (e.g. "BMS", "Charger") stay
// distinguishable on the console. `restart_retry_delay_ms` is the back-off
// between failed restart attempts.
inline void RecoverAfterError(I2CDriver* i2c, msg_t msg, const char* tag, uint32_t restart_retry_delay_ms) {
  // Distinguish the two recovery causes. MSG_TIMEOUT = the high-level bus
  // timeout (peripheral left in I2C_LOCKED). MSG_RESET + I2C_BUS_ERROR = the
  // in-ISR self-heal tripped on a direction desync (the IRQ storm).
  const i2cflags_t errs = i2cGetErrors(i2c);
  if (msg == MSG_TIMEOUT) {
    ULOG_WARNING("%s I2C TIMEOUT recovery (bus locked) - restarting I2C peripheral", tag);
  } else if ((errs & I2C_BUS_ERROR) != 0) {
    ULOG_WARNING("%s I2C ISR-STORM self-heal recovery (direction desync, errs=0x%02x) - restarting I2C peripheral", tag,
                 (unsigned)errs);
  } else {
    ULOG_WARNING("%s I2C error recovery (msg=%d errs=0x%02x) - restarting I2C peripheral", tag, (int)msg,
                 (unsigned)errs);
  }
  while (true) {
    const auto old_cfg = i2c->config;
    i2cStop(i2c);
    if (i2cStart(i2c, old_cfg) == HAL_RET_SUCCESS) {
      break;
    }
    ULOG_ERROR("%s I2C restart failed - retrying", tag);
    chThdSleepMilliseconds(restart_retry_delay_ms);
  }
}

// Wraps i2cMasterTransmitTimeout with bus-storm recovery, see RecoverAfterError.
inline msg_t TransmitTimeoutWithRecovery(I2CDriver* i2c, uint8_t addr, const uint8_t* tx, size_t tx_len, uint8_t* rx,
                                         size_t rx_len, sysinterval_t timeout, const char* tag,
                                         uint32_t restart_retry_delay_ms = 2) {
  const msg_t msg = i2cMasterTransmitTimeout(i2c, addr, tx, tx_len, rx, rx_len, timeout);
  if (msg != MSG_OK) {
    RecoverAfterError(i2c, msg, tag, restart_retry_delay_ms);
  }
  return msg;
}

// Wraps i2cMasterReceiveTimeout with bus-storm recovery, see RecoverAfterError.
inline msg_t ReceiveTimeoutWithRecovery(I2CDriver* i2c, uint8_t addr, uint8_t* rx, size_t rx_len, sysinterval_t timeout,
                                        const char* tag, uint32_t restart_retry_delay_ms = 2) {
  const msg_t msg = i2cMasterReceiveTimeout(i2c, addr, rx, rx_len, timeout);
  if (msg != MSG_OK) {
    RecoverAfterError(i2c, msg, tag, restart_retry_delay_ms);
  }
  return msg;
}

// Wraps i2cMasterTransmit (no timeout) with bus-storm recovery.
inline msg_t TransmitWithRecovery(I2CDriver* i2c, uint8_t addr, const uint8_t* tx, size_t tx_len, uint8_t* rx,
                                  size_t rx_len, const char* tag, uint32_t restart_retry_delay_ms = 2) {
  return TransmitTimeoutWithRecovery(i2c, addr, tx, tx_len, rx, rx_len, TIME_INFINITE, tag, restart_retry_delay_ms);
}

}  // namespace xbot::i2c

#endif  // XBOT_I2C_UTILS_HPP
