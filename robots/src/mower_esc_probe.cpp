#include "../include/mower_esc_probe.hpp"

#include <drivers/motor/yfr4esc/cobs.h>
#include <drivers/motor/yfr4esc/datatypes.h>
#include <ulog.h>

#include <cstdlib>
#include <cstring>
#include <drivers/motor/yfr4esc/crc16_hw.hpp>

namespace xbot::driver::motor {
namespace {

// One 40 ms window at 115200 baud carries ~300 bytes (the YFR4 streams a 49 byte frame roughly
// every 10 ms), 512 leaves headroom.
constexpr size_t kProbeBufferSize = 512;
constexpr uint32_t kProbeWindowMs = 40;
constexpr uint32_t kProbeWindows = 10;

// COBS worst case for one status frame (plus the stripped delimiter).
constexpr size_t kMaxEncodedFrameSize = sizeof(yfr4esc::StatusPacket) + sizeof(yfr4esc::StatusPacket) / 254 + 2;

/**
 * @brief Scratch memory for one probe run, taken from the heap for its duration only.
 *
 * The robot object is not allocated yet when the probe runs, so nothing competes for the heap - and
 * keeping ~620 bytes of .bss for a detection that runs once per boot would waste RAM for the whole
 * uptime. The default heap lives in AXI SRAM and is therefore reachable by the UART DMA.
 */
struct ProbeBuffers {
  uint8_t rx[kProbeBufferSize];
  uint8_t encoded[kMaxEncodedFrameSize];
  uint8_t decoded[sizeof(yfr4esc::StatusPacket)];
};

/**
 * @brief Look for one complete, CRC-valid YFR4 status frame inside @p data.
 *
 * Frames are COBS encoded and terminated by 0x00; a partially received leading frame simply fails the CRC check.
 */
bool FindStatusFrame(ProbeBuffers& buf, const uint8_t* data, size_t len, yfr4esc::StatusPacket& packet) {
  size_t encoded_len = 0;
  for (size_t i = 0; i < len; ++i) {
    const uint8_t byte = data[i];
    if (byte != 0) {
      if (encoded_len < sizeof(buf.encoded)) {
        buf.encoded[encoded_len++] = byte;
      } else {
        encoded_len = 0;  // garbage, drop it
      }
      continue;
    }

    // 0x00 is the frame delimiter (COBS never emits 0x00 inside a frame).
    if (encoded_len == 0) continue;
    const size_t decoded_len = cobs_decode(buf.encoded, encoded_len, buf.decoded);
    encoded_len = 0;
    if (decoded_len != sizeof(yfr4esc::StatusPacket)) continue;

    yfr4esc::StatusPacket candidate{};
    memcpy(&candidate, buf.decoded, sizeof(candidate));
    const uint16_t crc = yfr4esc_crc::crc16_ccitt_false(reinterpret_cast<const uint8_t*>(&candidate),
                                                        sizeof(candidate) - sizeof(candidate.crc));
    if (candidate.message_type == yfr4esc::STATUS && crc == candidate.crc) {
      packet = candidate;
      return true;
    }
  }
  return false;
}

MowerEscKind Probe(UARTDriver* uart) {
  auto* buf = static_cast<ProbeBuffers*>(std::malloc(sizeof(ProbeBuffers)));
  if (buf == nullptr) {
    ULOG_WARNING("Mower ESC probe: no heap for the receive buffers, assuming VESC");
    return MowerEscKind::VESC;
  }

  UARTConfig config{};
  config.speed = 115200;
  if (uartStart(uart, &config) != MSG_OK) {
    ULOG_ERROR("Mower ESC probe: could not start the mower UART, assuming VESC");
    std::free(buf);
    return MowerEscKind::VESC;
  }

  bool saw_data = false;
  bool found = false;
  for (uint32_t window = 0; window < kProbeWindows; ++window) {
    uartStartReceive(uart, kProbeBufferSize, buf->rx);
    chThdSleepMilliseconds(kProbeWindowMs);
    // uartStopReceive reports the bytes that were NOT received. UART_ERR_NOT_ACTIVE means the
    // reception had already finished (buffer full), i.e. everything arrived.
    const size_t not_received = uartStopReceive(uart);
    const size_t received =
        (not_received == UART_ERR_NOT_ACTIVE) ? kProbeBufferSize : (kProbeBufferSize - not_received);
    saw_data = saw_data || (received > 0);

    yfr4esc::StatusPacket packet{};
    if (received > 0 && FindStatusFrame(*buf, buf->rx, received, packet)) {
      ULOG_INFO("Mower ESC probe: YFR4 ESC answered (fw %u.%u, fault 0x%04X)", (unsigned)packet.fw_version_major,
                (unsigned)packet.fw_version_minor, (unsigned)packet.fault_code);
      found = true;
      break;
    }
  }
  uartStop(uart);
  std::free(buf);

  if (found) {
    return MowerEscKind::YFR4;
  }
  if (saw_data) {
    ULOG_WARNING("Mower ESC probe: data on the mower UART but no valid YFR4 frame, assuming VESC");
  } else {
    ULOG_INFO("Mower ESC probe: no unsolicited data on the mower UART, assuming VESC");
  }
  return MowerEscKind::VESC;
}

}  // namespace

MowerEscKind ProbeMowerEscOnce(UARTDriver* uart) {
  // Called from robot detection, before any driver owns the UART, so a plain cached value is enough
  static bool probed = false;
  static MowerEscKind result = MowerEscKind::VESC;
  if (!probed) {
    result = Probe(uart);
    probed = true;
  }
  return result;
}

}  // namespace xbot::driver::motor
