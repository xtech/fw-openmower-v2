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
   * @brief Start the YF Cover UI driver on the given UART port.
   *
   * Configures the UART at 115200 baud (with a character-match interrupt on the
   * COBS 0x00 delimiter), arms continuous DMA reception, and spawns the
   * communication thread. Using the UART driver (rather than the Serial driver)
   * keeps the firmware peripheral-agnostic: a board variant only has to pass a
   * different UARTDriver* (e.g. &UARTD6) without needing a different ChibiOS build.
   * Must be called from InitPlatform() before the scheduler starts.
   *
   * @param uart  Pointer to the ChibiOS UARTDriver (e.g. &UARTD7).
   */
  void Start(UARTDriver* uart);

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
  bool OnStart() override;

  // Protocol selection: must be configured via the "protocol" input config key.
  // UNCONFIGURED = no protocol specified → UART is disabled (OnStart logs a warning).
  // OM  = OpenMower COBS-based protocol.
  // OEM = OEM protocol (reserved, not yet implemented, treated as disabled).
  YFCoverUIProtocol protocol_ = YFCoverUIProtocol::UNCONFIGURED;

 private:
  uint8_t hall_mux_value_ = 0;  ///< 0=OM-XHST's or robot-adaptor plug, 1=OEM IDC
  // Extend the UART config with a back-pointer to this instance so the (static) UART
  // ISR callbacks can recover the owning driver from uartp->config.
  struct UARTConfigEx : UARTConfig {
    YFCoverUI* context;
  };
  UARTDriver* uart_ = nullptr;
  UARTConfigEx uart_config_{};

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

  // Continuous DMA receive buffer. The UART driver (unlike the Serial driver) does not
  // queue incoming bytes, so we receive into a circular DMA buffer and process NDTR deltas
  // on the comms thread. Sized to hold two full encoded frames so a wrap never overruns
  // an in-flight frame. Largest received message is sizeof(msg_event_rain)=12 bytes.
  static constexpr size_t RX_MAX_PKT_DECODED = sizeof(msg_event_rain);
  static constexpr size_t RX_MAX_PKT_CODED = RX_MAX_PKT_DECODED + (RX_MAX_PKT_DECODED / 254) + 1;  // COBS worst case
  static constexpr size_t DMA_RX_BUFFER_SIZE = 2 * (RX_MAX_PKT_CODED + 1);
  volatile uint8_t dma_rx_buffer_[DMA_RX_BUFFER_SIZE]{};
  size_t rx_seen_len_ = 0;  ///< how many bytes of dma_rx_buffer_ the thread has already consumed

  // COBS reassembly buffer (one encoded frame, delimiter excluded) and decoded payload buffer.
  static constexpr size_t RX_BUF_SIZE = 32;
  uint8_t rx_buf_[RX_BUF_SIZE]{};
  size_t rx_len_ = 0;
  static constexpr size_t DECODE_BUF_SIZE = 24;
  uint8_t decode_buf_[DECODE_BUF_SIZE]{};

  // DMA-safe TX buffer (avoid sending from the stack/DTCM). Only ever written/sent on the
  // comms thread, so no locking is required. Largest sent message is sizeof(msg_set_leds)=12.
  static constexpr size_t TX_BUF_SIZE = 32;
  uint8_t tx_buf_[TX_BUF_SIZE]{};

  // Thread
  THD_WORKING_AREA(wa_, 2048);
  thread_t* thread_ = nullptr;

  /**
   * @brief ChibiOS thread entry point trampoline.
   *
   * Casts the opaque @p instance back to a YFCoverUI and calls ThreadFunc().
   * @param instance  The YFCoverUI* passed to chThdCreateStatic().
   */
  static void ThreadHelper(void* instance);

  /**
   * @brief Main communication loop (never returns).
   *
   * Waits on RX events (DMA wrap / 0x00 character match) with a short timeout, drains
   * newly received DMA bytes via ProcessRxBytes(), runs the boot presence probe, polls
   * the panel for its version every 5 s, enforces the version-reply timeout, and sends
   * periodic LED updates at ui_interval_.
   */
  void ThreadFunc();

  // ---- Protocol TX ----
  /**
   * @brief COBS-encode a raw message into tx_buf_ and write it to the UART.
   *
   * Appends the 0x00 packet delimiter after the encoded payload. No-op if the
   * UART is not configured. Must only be called from the comms thread (tx_buf_
   * is shared without locking).
   * @param msg   Pointer to the raw (unencoded) message bytes.
   * @param size  Number of bytes to send from @p msg.
   */
  void SendMessage(const void* msg, size_t size);

  /** @brief Build and send a Get_Version request to poll the panel for presence. */
  void SendVersionRequest();

  /** @brief Build the current LED state (via BuildLedMessage()) and send it to the panel. */
  void SendLeds();

  /**
   * @brief Populate an LED message from current mower state.
   *
   * Maps battery/charge, high-level state, GPS quality, S1/S2 mode, and emergency
   * status onto the panel's LEDs and bar segments.
   * @param leds_msg  Message to fill; its CRC is computed by the caller.
   */
  void BuildLedMessage(msg_set_leds& leds_msg);

  // ---- Protocol RX ----
  /**
   * @brief Feed a run of received bytes through the COBS reassembler.
   *
   * Accumulates bytes into rx_buf_ until the 0x00 delimiter, then COBS-decodes the
   * frame and calls HandleMessage(). Called from the comms thread with freshly
   * arrived slices of the DMA receive buffer.
   * @param data  Pointer into dma_rx_buffer_ (volatile: written by DMA).
   * @param len   Number of new bytes to process.
   */
  void ProcessRxBytes(const volatile uint8_t* data, size_t len);

  /**
   * @brief Validate a decoded packet's CRC and dispatch it to the matching handler.
   *
   * Rejects packets shorter than 3 bytes or with a mismatched trailing CRC-16,
   * then routes by message type (version, button, emergency, rain, subscribe).
   * @param buf  Decoded message bytes (including the trailing 2-byte CRC).
   * @param len  Length of @p buf in bytes.
   */
  void HandleMessage(const uint8_t* buf, size_t len);

  /**
   * @brief Handle a version reply: mark the panel available and latch presence.
   * @param msg  The received version message.
   */
  void HandleVersion(const msg_get_version* msg);

  /**
   * @brief Handle a button event by injecting a press into matching button Inputs.
   * @param msg  The received button event (button_id and press_duration).
   */
  void HandleButton(const msg_event_button* msg);

  /**
   * @brief Handle an emergency event: store the new state bitmask and refresh inputs.
   * @param msg  The received emergency event carrying the Emergency_state bitmask.
   */
  void HandleEmergency(const msg_event_emergency* msg);

  /**
   * @brief Handle a rain event: update rain_detected_ from value vs. threshold.
   * @param msg  The received rain event (sensor value and threshold).
   */
  void HandleRain(const msg_event_rain* msg);

  /**
   * @brief Handle a subscribe event: adopt the requested LED update interval.
   * @param msg  The received subscribe event (topic bitmask and interval).
   */
  void HandleSubscribe(const msg_event_subscribe* msg);

  /** @brief Update all emergency Input objects from the current emergency_state_. */
  void UpdateEmergencyInputs();

  // ---- CRC & COBS ----
  /**
   * @brief Compute the CRC-16/CCITT over a byte range.
   * @param buf  Pointer to the bytes to checksum.
   * @param len  Number of bytes to include.
   * @return The 16-bit CRC value.
   */
  static uint16_t CalcCRC(const uint8_t* buf, size_t len);

  /**
   * @brief COBS-encode a buffer (does not append the 0x00 delimiter).
   * @param src      Source bytes to encode.
   * @param src_len  Number of source bytes.
   * @param dst      Destination buffer for the encoded output.
   * @return Number of bytes written to @p dst.
   */
  static size_t CobsEncode(const uint8_t* src, size_t src_len, uint8_t* dst);

  /**
   * @brief COBS-decode a buffer (the trailing 0x00 delimiter must already be stripped).
   * @param src      Encoded source bytes.
   * @param src_len  Number of source bytes.
   * @param dst      Destination buffer for the decoded output.
   * @param out_len  Set to the number of decoded bytes written to @p dst.
   * @return true on success, false on a malformed (truncated or zero-containing) packet.
   */
  static bool CobsDecode(const uint8_t* src, size_t src_len, uint8_t* dst, size_t& out_len);
};

}  // namespace xbot::driver::ui

#endif  // YF_COVER_UI_HPP
