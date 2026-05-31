/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "yf_cover_ui.hpp"

#include <EmergencyServiceBase.hpp>
#include <HighLevelServiceBase.hpp>
#include <etl/crc16_ccitt.h>
#include <etl/string_view.h>
#include <json_stream.hpp>
#include <ulog.h>

#include <services.hpp>

namespace xbot::driver::ui {

using namespace xbot::driver::input;

// ============================================================================
// Constants
// ============================================================================

static constexpr uint32_t VERSION_POLL_MS  = 5000;  ///< How often to poll UI version
static constexpr uint32_t VERSION_TIMEOUT_MS = 100; ///< How long to wait for version reply
static constexpr uint32_t LED_UPDATE_INTERVAL_DEFAULT_MS = 1000;

// ============================================================================
// Public
// ============================================================================

void YFCoverUI::Start(SerialDriver* sd) {
  if (thread_ != nullptr) {
    ULOG_ERROR("YFCoverUI: Start() called twice!");
    return;
  }
  if (sd == nullptr) {
    ULOG_ERROR("YFCoverUI: null SerialDriver!");
    return;
  }

  sd_ = sd;

  static const SerialConfig serial_cfg = {
      .speed = 115200,
      .cr1   = 0,
      .cr2   = USART_CR2_STOP1_BITS,
      .cr3   = 0,
  };
  sdStart(sd_, &serial_cfg);

  thread_ = chThdCreateStatic(&wa_, sizeof(wa_), NORMALPRIO - 1, ThreadHelper, this);
#ifdef USE_SEGGER_SYSTEMVIEW
  thread_->name = "YFCoverUI";
#endif
}

bool YFCoverUI::OnInputConfigValue(lwjson_stream_parser_t* jsp, const char* key,
                                      lwjson_stream_type_t type, Input& input) {
  if (strcmp(key, "channel") == 0) {
    JsonExpectType(STRING);
    etl::string_view ch{jsp->data.str.buff};
    if (ch == "latch")  { input.yf_cover_ui.channel = static_cast<uint8_t>(YFCoverUIChannel::LATCH);  return true; }
    if (ch == "stop1")  { input.yf_cover_ui.channel = static_cast<uint8_t>(YFCoverUIChannel::STOP1);  return true; }
    if (ch == "stop2")  { input.yf_cover_ui.channel = static_cast<uint8_t>(YFCoverUIChannel::STOP2);  return true; }
    if (ch == "lift")   { input.yf_cover_ui.channel = static_cast<uint8_t>(YFCoverUIChannel::LIFT);   return true; }
    if (ch == "liftx")  { input.yf_cover_ui.channel = static_cast<uint8_t>(YFCoverUIChannel::LIFTX);  return true; }
    if (ch == "button") {
      // Set the button flag bit; the button_id bits [6:0] will be filled by the "button_id" key.
      input.yf_cover_ui.channel = static_cast<uint8_t>(YFCoverUIChannel::BUTTON_FLAG);
      return true;
    }
    ULOG_ERROR("YFCoverUI: unknown channel \"%s\"", jsp->data.str.buff);
    return false;
  }
  if (strcmp(key, "button_id") == 0) {
    uint8_t button_id = 0;
    if (!JsonGetNumber(jsp, type, button_id)) return false;
    // Store the button_id in bits [6:0]; preserve bit 7 (BUTTON_FLAG) set by "channel":"button".
    input.yf_cover_ui.channel = static_cast<uint8_t>(YFCoverUIChannel::BUTTON_FLAG) | (button_id & 0x7F);
    return true;
  }
  ULOG_ERROR("YFCoverUI: unknown attribute \"%s\"", key);
  return false;
}

// ============================================================================
// Thread
// ============================================================================

void YFCoverUI::ThreadHelper(void* instance) {
  static_cast<YFCoverUI*>(instance)->ThreadFunc();
}

void YFCoverUI::ThreadFunc() {
  // Schedule the first version poll immediately and the first LED update after 500 ms.
  systime_t last_version_poll = chVTGetSystemTimeX() - TIME_MS2I(VERSION_POLL_MS);
  systime_t version_request   = 0;     // systime of the pending version request
  bool      version_pending   = false;  // true while awaiting a version response
  systime_t last_led_update   = chVTGetSystemTimeX() - TIME_MS2I(500);

  while (true) {
    // --- Read incoming bytes (10 ms timeout keeps the loop responsive) ---
    ReadByte(TIME_MS2I(10));

    // --- Version timeout: mark UI unavailable if no response within VERSION_TIMEOUT_MS ---
    if (version_pending && chVTTimeElapsedSinceX(version_request) >= TIME_MS2I(VERSION_TIMEOUT_MS)) {
      if (ui_available_.load()) {
        ULOG_INFO("YFCoverUI: UI timeout, marking unavailable");
        ui_available_.store(false);
        emergency_state_.store(0);
        UpdateEmergencyInputs();
      }
      version_pending = false;
    }

    // --- Periodic version poll (every 5 s) ---
    if (chVTTimeElapsedSinceX(last_version_poll) >= TIME_MS2I(VERSION_POLL_MS)) {
      SendVersionRequest();
      last_version_poll = chVTGetSystemTimeX();
      version_request   = last_version_poll;
      version_pending   = true;
    }

    // --- Periodic LED update ---
    if (chVTTimeElapsedSinceX(last_led_update) >= TIME_MS2I(ui_interval_)) {
      SendLeds();
      last_led_update = chVTGetSystemTimeX();
    }
  }
}

// ============================================================================
// Protocol TX
// ============================================================================

void YFCoverUI::SendMessage(const void* msg, size_t size) {
  if (sd_ == nullptr) return;

  // Encode with COBS into a local buffer (max encoded size = size + 2 overhead + 1 delimiter)
  static constexpr size_t ENC_BUF_SIZE = 32;
  uint8_t enc[ENC_BUF_SIZE];

  size_t enc_len = CobsEncode(static_cast<const uint8_t*>(msg), size, enc);
  enc[enc_len++] = 0x00;  // COBS packet delimiter

  sdWrite(sd_, enc, enc_len);
}

void YFCoverUI::SendVersionRequest() {
  msg_get_version msg{};
  msg.type    = Get_Version;
  msg.version = 0;
  msg.crc     = CalcCRC(reinterpret_cast<const uint8_t*>(&msg), sizeof(msg) - sizeof(msg.crc));
  SendMessage(&msg, sizeof(msg));
}

void YFCoverUI::SendLeds() {
  msg_set_leds leds_msg{};
  leds_msg.type = Set_LEDs;
  BuildLedMessage(leds_msg);
  leds_msg.crc = CalcCRC(reinterpret_cast<const uint8_t*>(&leds_msg),
                         sizeof(leds_msg) - sizeof(leds_msg.crc));
  SendMessage(&leds_msg, sizeof(leds_msg));
}

void YFCoverUI::BuildLedMessage(msg_set_leds& msg) {
  // ---- Battery / charging ----
  const float battery_volts  = power_service.GetBatteryVolts();
  const float adapter_volts  = power_service.GetAdapterVolts();
  const float charge_current = power_service.GetChargeCurrent();
  const float battery_pct    = power_service.GetBatteryPercent();  // 0.0–1.0

  // Charging LED: fast-blink=high current, slow-blink=trickle, on=charged/docked
  if (adapter_volts > 20.0f) {  // Docked (adapter present)
    if (charge_current > 0.5f) {
      setLed(msg, LED_CHARGING, LED_blink_fast);
    } else if (charge_current > 0.05f) {
      setLed(msg, LED_CHARGING, LED_blink_slow);
    } else {
      setLed(msg, LED_CHARGING, LED_on);  // Charged
    }
  } else {
    setLed(msg, LED_CHARGING, LED_off);
  }

  // Battery low LED
  const bool battery_low = !std::isnan(battery_pct) && battery_pct < 0.20f;
  setLed(msg, LED_BATTERY_LOW, battery_low ? LED_on : LED_off);

  // Battery bar (LED5–LED11, 7 segments)
  if (!std::isnan(battery_pct)) {
    setBars7(msg, static_cast<double>(battery_pct));
  }

  // ---- GPS quality ----
  const float gps_quality = high_level_service.GetGpsQuality();  // 0–100, or negative

  // GPS bar (LED15–LED18, 4 segments)
  if (std::isnan(gps_quality) || gps_quality < 0.0f) {
    setBars4(msg, -1.0);  // Fast-blink all = needs calibration / no fix
    setLed(msg, LED_POOR_GPS, LED_blink_fast);
  } else {
    setBars4(msg, static_cast<double>(gps_quality) / 100.0);
    setLed(msg, LED_POOR_GPS, gps_quality < 25.0f ? LED_on : LED_off);
  }

  // ---- High-level state (S1, S2 mode LEDs) ----
  const HighLevelStatus hl_status = high_level_service.GetStateId();
  const uint8_t SUBSTATE_SHIFT    = static_cast<uint8_t>(HighLevelStatus::SUBSTATE_SHIFT);
  const uint8_t STATE_MASK        = static_cast<uint8_t>((1 << SUBSTATE_SHIFT) - 1);
  const uint8_t SUBSTATE_MASK     = static_cast<uint8_t>(HighLevelStatus::SUBSTATE_MASK);

  const uint8_t hl_raw       = static_cast<uint8_t>(hl_status);
  const HighLevelStatus main_state = static_cast<HighLevelStatus>(hl_raw & STATE_MASK);
  const HighLevelStatus sub_state  = static_cast<HighLevelStatus>((hl_raw >> SUBSTATE_SHIFT) & SUBSTATE_MASK);

  // S1 = autonomous mowing
  if (main_state == HighLevelStatus::AUTONOMOUS && sub_state == HighLevelStatus::SUBSTATE_1) {
    setLed(msg, LED_S1, LED_on);
  } else if (main_state == HighLevelStatus::AUTONOMOUS) {
    setLed(msg, LED_S1, LED_blink_slow);  // Autonomous but not mowing (e.g. navigating)
  } else {
    setLed(msg, LED_S1, LED_off);
  }

  // S2 = docking / undocking
  if (main_state == HighLevelStatus::AUTONOMOUS &&
      (sub_state == HighLevelStatus::SUBSTATE_2 || sub_state == HighLevelStatus::SUBSTATE_3)) {
    setLed(msg, LED_S2, LED_on);
  } else if (main_state == HighLevelStatus::RECORDING) {
    setLed(msg, LED_S2, LED_blink_slow);  // Recording mode
  } else {
    setLed(msg, LED_S2, LED_off);
  }

  // ---- Emergency / mower-lifted LED ----
  const uint16_t emergency_reasons = emergency_service.GetEmergencyReasons();
  const bool stop_active   = (emergency_reasons & EmergencyReason::STOP) != 0;
  const bool lift_active   = (emergency_reasons & (EmergencyReason::LIFT | EmergencyReason::LIFT_MULTIPLE)) != 0;
  const bool latch_active  = (emergency_state_.load() & Emergency_latch) != 0;

  if (stop_active) {
    setLed(msg, LED_MOWER_LIFTED, LED_blink_fast);  // Stop button pressed
  } else if (lift_active) {
    setLed(msg, LED_MOWER_LIFTED, LED_blink_slow);  // Lifted
  } else if (latch_active) {
    setLed(msg, LED_MOWER_LIFTED, LED_on);           // Latched emergency
  } else {
    setLed(msg, LED_MOWER_LIFTED, LED_off);
  }

  (void)battery_volts;  // Available for future use (e.g. LOCK LED logic)
}

// ============================================================================
// Protocol RX
// ============================================================================

void YFCoverUI::ReadByte(sysinterval_t timeout) {
  msg_t byte = sdGetTimeout(sd_, timeout);
  if (byte == MSG_TIMEOUT || byte == MSG_RESET) {
    return;
  }

  const uint8_t b = static_cast<uint8_t>(byte);

  if (b == 0x00) {
    // End of COBS packet — decode and dispatch if we have data
    if (rx_len_ > 0) {
      size_t decoded_len = 0;
      if (CobsDecode(rx_buf_, rx_len_, decode_buf_, decoded_len)) {
        HandleMessage(decode_buf_, decoded_len);
      } else {
        ULOG_WARNING("YFCoverUI: COBS decode error (len=%u)", (unsigned)rx_len_);
      }
      rx_len_ = 0;
    }
  } else {
    if (rx_len_ < RX_BUF_SIZE) {
      rx_buf_[rx_len_++] = b;
    } else {
      // Buffer overflow — discard accumulated data and start over
      ULOG_WARNING("YFCoverUI: RX buffer overflow, discarding");
      rx_len_ = 0;
    }
  }
}

void YFCoverUI::HandleMessage(const uint8_t* buf, size_t len) {
  if (len < 1) return;

  // Validate CRC (last 2 bytes are the CRC, over all preceding bytes)
  if (len < 3) {
    ULOG_WARNING("YFCoverUI: message too short (%u bytes)", (unsigned)len);
    return;
  }
  const uint16_t received_crc = static_cast<uint16_t>(buf[len - 2]) |
                                 (static_cast<uint16_t>(buf[len - 1]) << 8);
  const uint16_t computed_crc = CalcCRC(buf, len - 2);
  if (received_crc != computed_crc) {
    ULOG_WARNING("YFCoverUI: CRC mismatch (got 0x%04X, expected 0x%04X)",
                 received_crc, computed_crc);
    return;
  }

  switch (buf[0]) {
    case Get_Version:
      if (len == sizeof(msg_get_version)) {
        HandleVersion(reinterpret_cast<const msg_get_version*>(buf));
      }
      break;
    case Get_Button:
      if (len == sizeof(msg_event_button)) {
        HandleButton(reinterpret_cast<const msg_event_button*>(buf));
      }
      break;
    case Get_Emergency:
      if (len == sizeof(msg_event_emergency)) {
        HandleEmergency(reinterpret_cast<const msg_event_emergency*>(buf));
      }
      break;
    case Get_Rain:
      if (len == sizeof(msg_event_rain)) {
        HandleRain(reinterpret_cast<const msg_event_rain*>(buf));
      }
      break;
    case Get_Subscribe:
      if (len == sizeof(msg_event_subscribe)) {
        HandleSubscribe(reinterpret_cast<const msg_event_subscribe*>(buf));
      }
      break;
    default:
      ULOG_WARNING("YFCoverUI: unknown message type 0x%02X", buf[0]);
      break;
  }
}

void YFCoverUI::HandleVersion(const msg_get_version* msg) {
  ULOG_DEBUG("YFCoverUI: version response, UI firmware v%u", msg->version);
  ui_available_.store(true);
}

void YFCoverUI::HandleButton(const msg_event_button* msg) {
  ULOG_DEBUG("YFCoverUI: button %u pressed (duration %u)", msg->button_id, msg->press_duration);

  const uint8_t btn_id = static_cast<uint8_t>(msg->button_id & 0x7F);
  const bool long_press = (msg->press_duration >= 1);

  // Inject press into any registered Input configured as a button with matching button_id.
  // Button inputs are identified by bit 7 of channel being set (BUTTON_FLAG).
  // Bits [6:0] of channel carry the configured button_id.
  for (auto& input : Inputs()) {
    if (YFCoverUIChannelIsButton(input.yf_cover_ui.channel) &&
        YFCoverUIChannelButtonId(input.yf_cover_ui.channel) == btn_id) {
      input.InjectPress(long_press);
    }
  }
}

void YFCoverUI::HandleEmergency(const msg_event_emergency* msg) {
  ULOG_DEBUG("YFCoverUI: emergency state 0x%02X", msg->state);
  emergency_state_.store(msg->state);
  UpdateEmergencyInputs();
}

void YFCoverUI::HandleRain(const msg_event_rain* msg) {
  const bool raining = (msg->value < msg->threshold);
  ULOG_DEBUG("YFCoverUI: rain sensor value=%lu threshold=%lu raining=%d",
             (unsigned long)msg->value, (unsigned long)msg->threshold, (int)raining);
  rain_detected_.store(raining);
}

void YFCoverUI::HandleSubscribe(const msg_event_subscribe* msg) {
  ULOG_DEBUG("YFCoverUI: subscribe topics=0x%02X interval=%u ms",
             msg->topic_bitmask, msg->interval);
  if (msg->interval > 0) {
    ui_interval_ = msg->interval;
  }
}

void YFCoverUI::UpdateEmergencyInputs() {
  const uint8_t state = emergency_state_.load();
  for (auto& input : Inputs()) {
    const uint8_t ch = input.yf_cover_ui.channel;
    if (!YFCoverUIChannelIsButton(ch)) {
      // Emergency channel (bit 7 = 0) — update based on corresponding bit in state
      const bool active = (state & (1u << ch)) != 0;
      input.Update(active);
    }
  }
}

// ============================================================================
// CRC-16/CCITT
// ============================================================================

uint16_t YFCoverUI::CalcCRC(const uint8_t* buf, size_t len) {
  etl::crc16_ccitt crc;
  crc.add(buf, buf + len);
  return crc.value();
}

// ============================================================================
// COBS encode / decode
// ============================================================================

size_t YFCoverUI::CobsEncode(const uint8_t* src, size_t src_len, uint8_t* dst) {
  size_t  dst_idx   = 0;
  size_t  code_idx  = 0;     // index of the "overhead" / code byte for the current block
  uint8_t code      = 0x01;  // distance to next 0x00 (or end of data + 1)

  dst[dst_idx++] = 0x00;  // placeholder for first overhead byte
  code_idx = dst_idx - 1;

  for (size_t i = 0; i < src_len; i++) {
    if (src[i] == 0x00) {
      // Write current code byte and start a new block
      dst[code_idx] = code;
      code_idx = dst_idx;
      dst[dst_idx++] = 0x01;
      code = 0x01;
    } else {
      dst[dst_idx++] = src[i];
      code++;
      if (code == 0xFF) {
        // Block of 254 non-zero bytes — start a new block
        dst[code_idx] = code;
        code_idx = dst_idx;
        dst[dst_idx++] = 0x01;
        code = 0x01;
      }
    }
  }
  dst[code_idx] = code;
  return dst_idx;
}

bool YFCoverUI::CobsDecode(const uint8_t* src, size_t src_len, uint8_t* dst, size_t& out_len) {
  out_len = 0;
  size_t i = 0;

  while (i < src_len) {
    const uint8_t code = src[i++];
    if (code == 0x00) {
      return false;  // Unexpected 0x00 inside a COBS packet
    }
    const uint8_t block = code - 1u;
    for (uint8_t j = 0; j < block; j++) {
      if (i >= src_len) return false;  // Truncated packet
      dst[out_len++] = src[i++];
    }
    // Append a 0x00 unless this was an "all-non-zero" block (code==0xFF) or end of input
    if (code != 0xFF && i < src_len) {
      dst[out_len++] = 0x00;
    }
  }
  return true;
}

}  // namespace xbot::driver::ui
