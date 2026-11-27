#ifndef MOWER_ESC_PROBE_HPP
#define MOWER_ESC_PROBE_HPP

#include <cstdint>

#include "hal.h"

namespace xbot::driver::motor {

/** Which protocol the ESC on the mower UART speaks. */
enum class MowerEscKind : uint8_t {
  VESC,  ///< VESC: answers polls only, never sends unsolicited data
  YFR4,  ///< YFR4 (YardForce-V4): streams its status frames at ~100 Hz
};

/**
 * @brief Identify the mower ESC protocol on @p uart by listening only (no TX).
 *
 * The YFR4 ESC streams status frames without being asked, a VESC only replies to polls. Up to
 * ten windows of 40 ms are listened to. The call returns as soon as one valid YFR4 status frame
 * (COBS framing, CRC16-CCITT-FALSE, 48 byte payload) was decoded.
 * Nothing valid means "assume VESC": a dead or unplugged YFR4 looks exactly like a VESC from the outside
 *
 * Blocking, so call it from robot detection before any driver owns @p uart.
 * Its scratch buffers (~620 B) are taken from the heap for the duration of the call,
 * the robot object is not allocated yet at that point, so nothing competes for the heap.
 * The result is cached, repeated calls are free (both YardForce variants ask during their detection).
 */
MowerEscKind ProbeMowerEscOnce(UARTDriver* uart);

}  // namespace xbot::driver::motor

#endif  // MOWER_ESC_PROBE_HPP
