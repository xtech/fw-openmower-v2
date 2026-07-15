/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "yf_cover_ui.hpp"

#include <etl/crc16_ccitt.h>
#include <etl/string_view.h>
#include <ulog.h>

#include <EmergencyServiceBase.hpp>
#include <HighLevelServiceBase.hpp>
#include <globals.hpp>
#include <json_stream.hpp>
#include <services.hpp>

namespace xbot::driver::ui {

using namespace xbot::driver::input;

// ============================================================================
// Constants
// ============================================================================

static constexpr uint32_t VERSION_POLL_MS = 5000;    ///< How often to poll UI version
static constexpr uint32_t VERSION_TIMEOUT_MS = 250;  ///< How long to wait for version reply
static constexpr uint32_t LED_UPDATE_INTERVAL_DEFAULT_MS = 1000;

static const ioline_t HALL_MUX_LINE = LINE_GPIO7;  ///< Hall- MUX selector GPIO

// Events signaled from the UART ISRs to the comms thread.
static constexpr eventmask_t EVT_RX_DMA_WRAP = (1U << 0);    ///< DMA receive buffer full / wrapped
static constexpr eventmask_t EVT_RX_CHAR_MATCH = (1U << 1);  ///< 0x00 COBS delimiter received

// ============================================================================
// Public
// ============================================================================

void YFCoverUI::Start(UARTDriver* uart) {
  if (thread_ != nullptr) {
    ULOG_ERROR("YFCoverUI: Start() called twice!");
    return;
  }
  if (uart == nullptr) {
    ULOG_ERROR("YFCoverUI: null UARTDriver!");
    return;
  }

  uart_ = uart;
  uart_config_.speed = 115200;  // 8N1 by default (cr1/cr2/cr3 left at 0 apart from char-match below)
  uart_config_.context = this;
  // Character-match interrupt on the COBS 0x00 frame delimiter, so the comms thread is
  // woken promptly when a complete packet has arrived.
  uart_config_.cr2 = ('\0' << USART_CR2_ADD_Pos);
  uart_config_.cr1 |= USART_CR1_CMIE;

  // RX buffer-full (DMA wrap) callback: re-arm DMA immediately to minimise the RX gap,
  // then wake the comms thread to drain the buffer.
  uart_config_.rxend_cb = [](UARTDriver* uartp) {
    chSysLockFromISR();
    YFCoverUI* instance = reinterpret_cast<const UARTConfigEx*>(uartp->config)->context;
    chDbgAssert(instance != nullptr, "instance cannot be null!");
    uartStartReceiveI(uartp, DMA_RX_BUFFER_SIZE, const_cast<uint8_t*>(instance->dma_rx_buffer_));
    if (instance->thread_) chEvtSignalI(instance->thread_, EVT_RX_DMA_WRAP);
    chSysUnlockFromISR();
  };

  // Character-match callback: only wakes the thread; byte processing happens there.
  uart_config_.rx_cm_cb = [](UARTDriver* uartp) {
    chSysLockFromISR();
    YFCoverUI* instance = reinterpret_cast<const UARTConfigEx*>(uartp->config)->context;
    chDbgAssert(instance != nullptr, "instance cannot be null!");
    if (instance->thread_) chEvtSignalI(instance->thread_, EVT_RX_CHAR_MATCH);
    chSysUnlockFromISR();
  };

  if (uartStart(uart_, &uart_config_) != MSG_OK) {
    ULOG_ERROR("YFCoverUI: uartStart() failed!");
    uart_ = nullptr;
    return;
  }

  thread_ = chThdCreateStatic(&wa_, sizeof(wa_), NORMALPRIO - 1, ThreadHelper, this);
#ifdef USE_SEGGER_SYSTEMVIEW
  thread_->name = "YFCoverUI";
#endif

  // Arm continuous DMA reception (thread_ is set first so the ISRs can signal it).
  rx_seen_len_ = 0;
  uartStartReceive(uart_, DMA_RX_BUFFER_SIZE, const_cast<uint8_t*>(dma_rx_buffer_));
}

bool YFCoverUI::OnInputConfigValue(lwjson_stream_parser_t* jsp, const char* key, lwjson_stream_type_t type,
                                   Input& input) {
  if (strcmp(key, "channel") == 0) {
    JsonExpectType(STRING);
    etl::string_view ch{jsp->data.str.buff};
    if (ch == "latch") {
      input.yf_cover_ui.channel = static_cast<uint8_t>(YFCoverUIChannel::LATCH);
      return true;
    }
    if (ch == "stop1") {
      input.yf_cover_ui.channel = static_cast<uint8_t>(YFCoverUIChannel::STOP1);
      return true;
    }
    if (ch == "stop2") {
      input.yf_cover_ui.channel = static_cast<uint8_t>(YFCoverUIChannel::STOP2);
      return true;
    }
    if (ch == "lift") {
      input.yf_cover_ui.channel = static_cast<uint8_t>(YFCoverUIChannel::LIFT);
      return true;
    }
    if (ch == "liftx") {
      input.yf_cover_ui.channel = static_cast<uint8_t>(YFCoverUIChannel::LIFTX);
      return true;
    }
    if (ch == "button") {
      // Set the button flag bit while preserving any button_id bits [6:0] already
      // parsed (JSON key order is not guaranteed, so "button_id" may precede "channel").
      input.yf_cover_ui.channel =
          static_cast<uint8_t>(YFCoverUIChannel::BUTTON_FLAG) | (input.yf_cover_ui.channel & 0x7F);
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

  // Global driver configuration.
  // id/value pair: id identifies which setting, value provides the parameter.
  if (strcmp(key, "id") == 0) {
    JsonExpectType(STRING);
    if (strcmp(jsp->data.str.buff, "hall_mux") == 0) {
      input.yf_cover_ui.flags |= Input::YF_FLAG_HALL_MUX;
      return true;
    }
    if (strcmp(jsp->data.str.buff, "protocol") == 0) {
      input.yf_cover_ui.flags |= Input::YF_FLAG_PROTOCOL;
      return true;
    }
    ULOG_ERROR("YFCoverUI: unknown id \"%s\"", jsp->data.str.buff);
    return false;
  }
  if (strcmp(key, "value") == 0) {
    JsonExpectType(STRING);
    if (input.yf_cover_ui.flags & Input::YF_FLAG_PROTOCOL) {
      if (strcmp(jsp->data.str.buff, "om") == 0) {
        protocol_.store(YFCoverUIProtocol::OM);
        return true;
      }
      if (strcmp(jsp->data.str.buff, "oem") == 0) {
        protocol_.store(YFCoverUIProtocol::OEM);
        ULOG_WARNING("YFCoverUI: OEM protocol not yet implemented, disabling UART");
        return true;
      }
      ULOG_ERROR("YFCoverUI: unknown protocol value \"%s\"", jsp->data.str.buff);
      return false;
    } else if (input.yf_cover_ui.flags & Input::YF_FLAG_HALL_MUX) {
      if (strcmp(jsp->data.str.buff, "om") == 0) {
        hall_mux_value_ = 0;
        return true;
      }
      if (strcmp(jsp->data.str.buff, "oem_idc") == 0) {
        hall_mux_value_ = 1;
        return true;
      }
      ULOG_ERROR("YFCoverUI: unknown hall_mux value \"%s\"", jsp->data.str.buff);
      return false;
    }
    // "value" parsed before "id" — flags not set yet
    ULOG_ERROR("YFCoverUI: id not set before value");
    return false;
  }
  ULOG_ERROR("YFCoverUI: unknown attribute \"%s\"", key);
  return false;
}

bool YFCoverUI::OnStart() {
  const bool is_pre_v120 = carrier_board_info.version_major < 1 ||
                           (carrier_board_info.version_major == 1 && carrier_board_info.version_minor < 2);

  // Refuse OEM IDC on pre-1.2.0 boards that don't have the MUX hardware
  if (is_pre_v120 && hall_mux_value_ != 0) {
    ULOG_ERROR("YFCoverUI: hall_mux is set to OEM IDC but carrier board is pre v1.2.0 and don't have the MUX hardware");
    return false;
  }

  // Enforce protocol configuration: must be explicitly set to "om" for UART to operate
  if (protocol_ != YFCoverUIProtocol::OM) {
    if (protocol_ == YFCoverUIProtocol::UNCONFIGURED) {
      ULOG_WARNING("YFCoverUI: no protocol configured, disabling UART");
    }
    if (uart_ != nullptr) uartStopReceive(uart_);
  }

  // Always set MUX for v1.2.0+ (even if no hall_mux was configured, default to OM-XHST/robot-adaptor plug)
  if (!is_pre_v120) {
    palSetLineMode(HALL_MUX_LINE, PAL_MODE_OUTPUT_PUSHPULL);
    palWriteLine(HALL_MUX_LINE, hall_mux_value_);
  }
  return true;
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
  systime_t version_request = 0;  // systime of the pending version request
  systime_t last_led_update = chVTGetSystemTimeX() - TIME_MS2I(500);

  // Boot presence probe: if no response arrives within PRESENCE_PROBE_MS, declare the
  // panel "not found" once. HandleVersion() logs "connected" if it responds first.
  const systime_t probe_start = chVTGetSystemTimeX();

  while (true) {
    // If protocol is not OM, skip all UART activity (OnStart() already stopped receive)
    if (protocol_ != YFCoverUIProtocol::OM) {
      chThdSleepMilliseconds(100);
      continue;
    }

    // --- Wait for RX events; the 100 ms timeout keeps the periodic poll/LED timing responsive ---
    const eventmask_t evt = chEvtWaitAnyTimeout(EVT_RX_DMA_WRAP | EVT_RX_CHAR_MATCH, TIME_MS2I(100));

    // --- Drain newly received DMA bytes ---
    // EVT_RX_DMA_WRAP signals that rxend_cb fired and re-armed the DMA buffer.
    // Handle the wrap explicitly from the event rather than inferring it from NDTR: by the time
    // NDTR is read, re-arming has already reset it, so received_so_far >= rx_seen_len_ even
    // after a wrap, making the NDTR-delta inference unreliable.

    if (evt & EVT_RX_DMA_WRAP) {
      ProcessRxBytes(dma_rx_buffer_ + rx_seen_len_, DMA_RX_BUFFER_SIZE - rx_seen_len_);
      rx_seen_len_ = 0;
    }
    const uint32_t ndtr_now = uart_->dmarx->stream->NDTR;
    if (ndtr_now <= DMA_RX_BUFFER_SIZE) {
      const size_t received_so_far = DMA_RX_BUFFER_SIZE - ndtr_now;
      if (received_so_far > rx_seen_len_) {
        ProcessRxBytes(dma_rx_buffer_ + rx_seen_len_, received_so_far - rx_seen_len_);
        rx_seen_len_ = received_so_far;
      }
    }

    // --- Startup presence verdict (logged exactly once) ---
    if (!presence_determined_.load() && !ui_present_.load() &&
        chVTTimeElapsedSinceX(probe_start) >= TIME_MS2I(PRESENCE_PROBE_MS)) {
      ULOG_WARNING("yf_cover_ui not found");
      presence_determined_.store(true);
    }

    // --- Version timeout ---
    // HandleVersion() clears version_pending_ as soon as a reply arrives, so this only
    // fires when the panel genuinely failed to answer the last poll.
    if (version_pending_ && chVTTimeElapsedSinceX(version_request) >= TIME_MS2I(VERSION_TIMEOUT_MS)) {
      if (ui_available_.load()) {
        ULOG_INFO("YFCoverUI: UI timeout, marking unavailable");
        ui_available_.store(false);
        emergency_state_.store(0);
        UpdateEmergencyInputs();
      }
      version_pending_ = false;
    }

    // --- Periodic version poll (every 5 s) ---
    if (chVTTimeElapsedSinceX(last_version_poll) >= TIME_MS2I(VERSION_POLL_MS)) {
      SendVersionRequest();
      last_version_poll = chVTGetSystemTimeX();
      version_request = last_version_poll;
      version_pending_ = true;
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
  // Verify at compile time that every known protocol message fits in tx_buf_ after COBS encoding.
  static constexpr size_t kMaxMsg =
      std::max(sizeof(msg_get_version),
               std::max(sizeof(msg_set_buzzer),
                        std::max(sizeof(msg_set_leds),
                                 std::max(sizeof(msg_event_button),
                                          std::max(sizeof(msg_event_emergency),
                                                   std::max(sizeof(msg_event_rain), sizeof(msg_event_subscribe)))))));
  static_assert(kMaxMsg + (kMaxMsg / 254) + 2 <= TX_BUF_SIZE,
                "TX_BUF_SIZE too small for largest protocol message after COBS encoding");

  if (uart_ == nullptr) return;

  // COBS worst-case overhead: 1 extra byte per 254 input bytes, rounded up, plus 1 delimiter.
  if (size + (size / 254) + 2 > TX_BUF_SIZE) {
    ULOG_WARNING("YFCoverUI: TX frame too large (%u)", (unsigned)size);
    return;
  }
  size_t enc_len = CobsEncode(static_cast<const uint8_t*>(msg), size, tx_buf_);
  tx_buf_[enc_len++] = 0x00;  // COBS packet delimiter

  uartSendFullTimeout(uart_, &enc_len, tx_buf_, TIME_INFINITE);
}

void YFCoverUI::SendVersionRequest() {
  msg_get_version msg{};
  msg.type = Get_Version;
  msg.version = 0;
  msg.crc = CalcCRC(reinterpret_cast<const uint8_t*>(&msg), sizeof(msg) - sizeof(msg.crc));
  SendMessage(&msg, sizeof(msg));
}

void YFCoverUI::SendLeds() {
  msg_set_leds leds_msg{};
  leds_msg.type = Set_LEDs;
  BuildLedMessage(leds_msg);
  leds_msg.crc = CalcCRC(reinterpret_cast<const uint8_t*>(&leds_msg), sizeof(leds_msg) - sizeof(leds_msg.crc));
  SendMessage(&leds_msg, sizeof(leds_msg));
}

void YFCoverUI::BuildLedMessage(msg_set_leds& msg) {
  // ---- Battery / charging ----
  const float battery_volts = power_service.GetBatteryVolts();
  const float adapter_volts = power_service.GetAdapterVolts();
  const float charge_current = power_service.GetChargeCurrent();
  const float battery_pct = power_service.GetBatteryPercent();  // 0.0–1.0

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

  // ---- High-level state ----
  const HighLevelStatus hl_status = high_level_service.GetStateId();
  const uint8_t SUBSTATE_SHIFT = static_cast<uint8_t>(HighLevelStatus::SUBSTATE_SHIFT);
  const uint8_t STATE_MASK = static_cast<uint8_t>((1 << SUBSTATE_SHIFT) - 1);
  const uint8_t SUBSTATE_MASK = static_cast<uint8_t>(HighLevelStatus::SUBSTATE_MASK);

  const uint8_t hl_raw = static_cast<uint8_t>(hl_status);
  const HighLevelStatus main_state = static_cast<HighLevelStatus>(hl_raw & STATE_MASK);
  const HighLevelStatus sub_state = static_cast<HighLevelStatus>((hl_raw >> SUBSTATE_SHIFT) & SUBSTATE_MASK);
  const bool docked = (main_state == HighLevelStatus::IDLE);

  // ---- GPS quality (LED15–LED18, 4 segments) ----
  // mower_logic publishes gps_quality_percent as a 0.0–1.0 fraction (1.0 = best),
  // 0.0 = bad/timeout, negative = no fix / no orientation.
  // The bar is only fully off while docked (idle); otherwise it shows the fix
  // quality and blinks while reception is bad/absent, so it is never silently dark.
  const float gps_quality = high_level_service.GetGpsQuality();
  if (docked) {
    setBars4(msg, 0.0);  // docked -> GPS bar off
    setLed(msg, LED_POOR_GPS, LED_off);
  } else if (std::isnan(gps_quality) || gps_quality < 0.25f) {
    setBars4(msg, -1.0);  // blink all 4 -> poor reception / no fix
    setLed(msg, LED_POOR_GPS, LED_blink_fast);
  } else {
    setBars4(msg, static_cast<double>(gps_quality));  // proportional, good reception
    setLed(msg, LED_POOR_GPS, LED_off);
  }

  // ---- S1 / S2 mode LEDs ----
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
  // Blink while a physical emergency condition is currently active (e.g. wheel
  // tilted / lift sensor, stop button, collision). Once that condition clears but
  // the emergency is still latched, switch to steady on until it is reset. Off
  // when there is no emergency. reasons_ keeps the live condition bits (which clear
  // when the wheel is set down) plus a sticky LATCH bit (cleared only on reset).
  const uint16_t emergency_reasons = emergency_service.GetEmergencyReasons();
  const bool condition_active =
      (emergency_reasons & (EmergencyReason::STOP | EmergencyReason::LIFT | EmergencyReason::LIFT_MULTIPLE |
                            EmergencyReason::COLLISION)) != 0;
  const bool latched = (emergency_reasons & EmergencyReason::LATCH) != 0;

  if (condition_active) {
    setLed(msg, LED_MOWER_LIFTED, LED_blink_fast);  // condition present (e.g. wheel tilted)
  } else if (latched) {
    setLed(msg, LED_MOWER_LIFTED, LED_on);  // latched, condition cleared, awaiting reset
  } else {
    setLed(msg, LED_MOWER_LIFTED, LED_off);  // no emergency
  }

  (void)battery_volts;  // Available for future use (e.g. LOCK LED logic)
}

// ============================================================================
// Protocol RX
// ============================================================================

void YFCoverUI::ProcessRxBytes(const volatile uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    const uint8_t b = data[i];
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
}

void YFCoverUI::HandleMessage(const uint8_t* buf, size_t len) {
  if (len < 1) return;

  // Validate CRC (last 2 bytes are the CRC, over all preceding bytes)
  if (len < 3) {
    ULOG_WARNING("YFCoverUI: message too short (%u bytes)", (unsigned)len);
    return;
  }
  const uint16_t received_crc = static_cast<uint16_t>(buf[len - 2]) | (static_cast<uint16_t>(buf[len - 1]) << 8);
  const uint16_t computed_crc = CalcCRC(buf, len - 2);
  if (received_crc != computed_crc) {
    ULOG_WARNING("YFCoverUI: CRC mismatch (got 0x%04X, expected 0x%04X)", received_crc, computed_crc);
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
    default: ULOG_WARNING("YFCoverUI: unknown message type 0x%02X", buf[0]); break;
  }
}

void YFCoverUI::HandleVersion(const msg_get_version* msg) {
  ULOG_DEBUG("YFCoverUI: version response, UI firmware v%u", msg->version);
  version_pending_ = false;  // reply received -> cancel the pending-timeout
  ui_available_.store(true);
  // Latch presence on the first ever response and announce it once.
  // ui_present_ is written only here (comms thread), so load-then-store is race-free.
  if (!ui_present_.load()) {
    ui_present_.store(true);
    presence_determined_.store(true);
    ULOG_WARNING("yf_cover_ui connected (UI firmware v%u)", msg->version);
  }
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
  ULOG_DEBUG("YFCoverUI: rain sensor value=%lu threshold=%lu raining=%d", (unsigned long)msg->value,
             (unsigned long)msg->threshold, (int)raining);
  rain_detected_.store(raining);
}

void YFCoverUI::HandleSubscribe(const msg_event_subscribe* msg) {
  ULOG_DEBUG("YFCoverUI: subscribe topics=0x%02X interval=%u ms", msg->topic_bitmask, msg->interval);
  if (msg->interval >= 100 && msg->interval <= 10000) {
    ui_interval_ = msg->interval;
  } else if (msg->interval > 0) {
    ULOG_WARNING("YFCoverUI: ignoring out-of-range interval %u ms", msg->interval);
  }
}

void YFCoverUI::UpdateEmergencyInputs() {
  const uint8_t state = emergency_state_.load();
  for (auto& input : Inputs()) {
    if (input.yf_cover_ui.flags & (Input::YF_FLAG_HALL_MUX | Input::YF_FLAG_PROTOCOL))
      continue;  // skip setup-only entries
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
  size_t dst_idx = 0;
  size_t code_idx = 0;  // index of the "overhead" / code byte for the current block
  uint8_t code = 0x01;  // distance to next 0x00 (or end of data + 1)

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
