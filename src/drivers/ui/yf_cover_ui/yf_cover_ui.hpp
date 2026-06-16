/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file yf_cover_ui.hpp
 * @brief Driver for the YardForce Cover UI (yf_cover_ui) panel on the YardForce 500 mower.
 *
 * The YF Cover UI board communicates via UART7 at 115200 baud using COBS packet framing
 * and CRC-16/CCITT error detection. This driver:
 *  - Polls the YF Cover UI for presence every 5 s
 *  - Sends LED state updates at a configurable interval (default 1 s)
 *  - Forwards button events and emergency signals to the InputService
 *  - Reads rain sensor data from the YF Cover UI board
 *
 * The driver extends InputDriver so that emergency and button inputs are routed
 * through the standard InputService / EmergencyService pipeline.
 */

#ifndef YF_COVER_UI_HPP
#define YF_COVER_UI_HPP

#include <etl/atomic.h>
#include <hal.h>
#include <lwjson/lwjson.h>

#include <drivers/input/input_driver.hpp>

#include "yf_cover_ui_protocol.hpp"

namespace xbot::driver::ui {

using namespace xbot::driver::input;

/**
 * @brief Channel encoding for YF Cover UI inputs, stored in Input::yf_cover_ui.channel.
 *
 * Bit layout:
 *   bit 7 = 0 → emergency channel; bits [4:0] = Emergency_state bit position (0–4)
 *   bit 7 = 1 → button channel;    bits [6:0] = button_id from msg_event_button
 *
 * Emergency channels are updated by HandleEmergency() from the Emergency_state bitmask.
 * Button channels are triggered by HandleButton() when button_id matches bits [6:0].
 *
 * Use BUTTON_FLAG (0x80) OR'd with the button_id to form a button channel value.
 * Use YFCoverUIChannelIsButton() and YFCoverUIChannelButtonId() helpers to test and extract.
 */
enum class YFCoverUIChannel : uint8_t {
  // Emergency channels — map to Emergency_state bits (bit 7 = 0)
  LATCH = 0,  ///< Emergency latch (bit 0 of Emergency_state)
  STOP1 = 1,  ///< Stop button 1  (bit 1 of Emergency_state)
  STOP2 = 2,  ///< Stop button 2  (bit 2 of Emergency_state)
  LIFT = 3,   ///< Lift sensor 1  (bit 3 of Emergency_state)
  LIFTX = 4,  ///< Lift sensor 2  (bit 4 of Emergency_state)
  // Button channels — bit 7 set, lower 7 bits carry the button_id
  BUTTON_FLAG = 0x80,  ///< OR with button_id to encode a button channel
};

/** @return true if the encoded channel byte represents a button (not an emergency). */
constexpr bool YFCoverUIChannelIsButton(uint8_t ch) {
  return (ch & 0x80) != 0;
}
/** @return the button_id stored in an encoded button channel byte. */
constexpr uint8_t YFCoverUIChannelButtonId(uint8_t ch) {
  return ch & 0x7F;
}

class YFCoverUI : public InputDriver {
 public:
  /**
   * @brief Start the YF Cover UI driver on the given serial port.
   *
   * Configures SD7 at 115200 baud and spawns the communication thread.
   * Must be called from InitPlatform() before the scheduler starts.
   *
   * @param sd  Pointer to the ChibiOS SerialDriver (use &SD7).
   */
  void Start(SerialDriver* sd);

  /** @return true if the YF Cover UI panel is connected and responding. */
  bool IsAvailable() const {
    return ui_available_.load();
  }

  /** @return true if the panel has ever responded since boot (latched; survives transient timeouts). */
  bool IsPresent() const {
    return ui_present_.load();
  }

  /** @return true if rain is currently detected by the YF Cover UI rain sensor. */
  bool IsRainDetected() const {
    return rain_detected_.load();
  }

  // ---- InputDriver interface ----

  /**
   * @brief Parse input configuration from the JSON sent by open_mower_ros.
   *
   * Recognised keys:
   *  - "channel": "latch"|"stop1"|"stop2"|"lift"|"liftx"
   *               → emergency channel; stores bit position (0–4) in channel byte.
   *  - "channel": "button"
   *               → button channel; sets bit 7 in channel byte.
   *               Requires a subsequent "button_id" key.
   *  - "button_id": <uint> (0–127)
   *               → button_id sent by the YF Cover UI in msg_event_button.
   *               Must be combined with "channel":"button".
   *               Stored in bits [6:0] of the channel byte (bit 7 remains set).
   */
  bool OnInputConfigValue(lwjson_stream_parser_t* jsp, const char* key, lwjson_stream_type_t type,
                          Input& input) override;

 private:
  SerialDriver* sd_ = nullptr;

  etl::atomic<bool> ui_available_{false};    ///< true while the panel is currently responding
  etl::atomic<bool> ui_present_{false};      ///< latched: true once the panel has responded at least once
  etl::atomic<uint8_t> emergency_state_{0};  ///< Last received emergency bitmask
  etl::atomic<bool> rain_detected_{false};
  uint16_t ui_interval_ = 1000;  ///< LED update interval in ms (from Subscribe msg)

  // Version handshake state. Only touched on the comms thread (ThreadFunc polls,
  // HandleVersion() clears on a reply), so a plain bool is safe.
  bool version_pending_ = false;  ///< true while awaiting a Get_Version reply

  // Startup presence detection: probe the panel for a bounded window at boot, then
  // log "connected" / "not found" exactly once. Until presence is determined we still
  // accept ROS input config; once determined absent, panel inputs are silently ignored.
  static constexpr uint32_t PRESENCE_PROBE_MS = 3000;  ///< How long to wait at boot for a first response
  // Written by the comms thread, read by the input-service thread in OnInputConfigValue() -> atomic.
  etl::atomic<bool> presence_determined_{false};  ///< presence verdict has been reached and logged

  // COBS receive buffer. Maximum unencoded message is sizeof(msg_event_rain)=12, so 20 bytes
  // of encoded data is plenty with the COBS overhead byte and delimiter.
  static constexpr size_t RX_BUF_SIZE = 32;
  uint8_t rx_buf_[RX_BUF_SIZE]{};
  size_t rx_len_ = 0;

  // Decoded payload buffer
  static constexpr size_t DECODE_BUF_SIZE = 24;
  uint8_t decode_buf_[DECODE_BUF_SIZE]{};

  // Thread
  THD_WORKING_AREA(wa_, 2048);
  thread_t* thread_ = nullptr;

  static void ThreadHelper(void* instance);
  void ThreadFunc();

  // ---- Protocol TX ----
  void SendMessage(const void* msg, size_t size);
  void SendVersionRequest();
  void SendLeds();
  void BuildLedMessage(msg_set_leds& leds_msg);

  // ---- Protocol RX ----
  /**
   * @brief Read one byte from the serial port (with timeout) and feed COBS decoder.
   *
   * When a complete packet (terminated by 0x00) is received, calls HandleMessage().
   * @param timeout_ms  sdGetTimeout() timeout in milliseconds.
   */
  void ReadByte(sysinterval_t timeout_ms);
  void HandleMessage(const uint8_t* buf, size_t len);
  void HandleVersion(const msg_get_version* msg);
  void HandleButton(const msg_event_button* msg);
  void HandleEmergency(const msg_event_emergency* msg);
  void HandleRain(const msg_event_rain* msg);
  void HandleSubscribe(const msg_event_subscribe* msg);

  /** @brief Update all emergency Input objects from the current emergency_state_. */
  void UpdateEmergencyInputs();

  // ---- CRC & COBS ----
  static uint16_t CalcCRC(const uint8_t* buf, size_t len);
  static size_t CobsEncode(const uint8_t* src, size_t src_len, uint8_t* dst);
  static bool CobsDecode(const uint8_t* src, size_t src_len, uint8_t* dst, size_t& out_len);
};

}  // namespace xbot::driver::ui

#endif  // YF_COVER_UI_HPP
