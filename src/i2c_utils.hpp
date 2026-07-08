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

// Wraps i2cMasterTransmit with bus-storm recovery. A bus timeout leaves the
// peripheral in the I2C_LOCKED state and it must be restarted (i2cStop +
// i2cStart) before it can be used again; this restarts it and loops until the
// restart succeeds. The caller must already hold the I2C bus.
//
// `tag` prefixes the log lines so callers (e.g. "BMS", "Charger") stay
// distinguishable on the console. `restart_retry_delay_ms` is the back-off
// between failed restart attempts.
inline msg_t TransmitWithRecovery(I2CDriver* i2c, uint8_t addr, const uint8_t* tx, size_t tx_len, uint8_t* rx,
                                  size_t rx_len, const char* tag, uint32_t restart_retry_delay_ms = 2) {
  const msg_t msg = i2cMasterTransmit(i2c, addr, tx, tx_len, rx, rx_len);
  if (msg != MSG_OK) {
    // Distinguish the two recovery causes. MSG_TIMEOUT = the high-level bus
    // timeout (peripheral left in I2C_LOCKED). MSG_RESET + I2C_BUS_ERROR = the
    // in-ISR self-heal tripped on a direction desync (the IRQ storm).
    const i2cflags_t errs = i2cGetErrors(i2c);
    if (msg == MSG_TIMEOUT) {
      ULOG_WARNING("%s I2C TIMEOUT recovery (bus locked) - restarting I2C peripheral", tag);
    } else if ((errs & I2C_BUS_ERROR) != 0) {
      ULOG_WARNING("%s I2C ISR-STORM self-heal recovery (direction desync, errs=0x%02x) - restarting I2C peripheral",
                   tag, (unsigned)errs);
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
  return msg;
}

}  // namespace xbot::i2c

#endif  // XBOT_I2C_UTILS_HPP
