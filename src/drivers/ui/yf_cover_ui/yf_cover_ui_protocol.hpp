/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file yf_cover_ui_protocol.hpp
 * @brief Protocol definitions for the YardForce Cover UI (yf_cover_ui) (Yardforce 500 type).
 *
 * This protocol is used by the Yardforce 500 mower's front panel (YF Cover UI board).
 * Communication is UART at 115200 baud with COBS packet framing and CRC-16/CCITT.
 *
 * Ported from OpenMower/Firmware/LowLevel/src/ui_board.h (original by Elmar Elflein).
 */

#ifndef YF_COVER_UI_PROTOCOL_HPP
#define YF_COVER_UI_PROTOCOL_HPP

#include <cmath>
#include <cstdint>

// ---- Message type identifiers ----

enum YFCoverUIType : uint8_t {
  Get_Version   = 0xB0,  ///< Version exchange / UI presence check (bidirectional)
  Set_Buzzer    = 0xB1,  ///< Firmware → UI: control buzzer pattern
  Set_LEDs      = 0xB2,  ///< Firmware → UI: update 18 LED states
  Get_Button    = 0xB3,  ///< UI → Firmware: button press event
  Get_Emergency = 0xB4,  ///< UI → Firmware: emergency button / sensor states (YardForce Cover UI)
  Get_Rain      = 0xB5,  ///< UI → Firmware: rain sensor reading (YardForce Cover UI)
  Get_Subscribe = 0xB6,  ///< UI → Firmware: subscribe to periodic data topics
};

// ---- LED identifiers (18 LEDs total) ----

enum LED_id : uint8_t {
  LED_CHARGING    = 0,   ///< Charging status indicator
  LED_BATTERY_LOW = 1,   ///< Low battery warning
  LED_POOR_GPS    = 2,   ///< Poor GPS quality indicator
  LED_MOWER_LIFTED = 3,  ///< Mower lifted / emergency indicator
  LED5  = 4,   // Battery bar segment 7 (lowest)
  LED6  = 5,   // Battery bar segment 6
  LED7  = 6,   // Battery bar segment 5
  LED8  = 7,   // Battery bar segment 4
  LED9  = 8,   // Battery bar segment 3
  LED10 = 9,   // Battery bar segment 2
  LED11 = 10,  // Battery bar segment 1 (highest)
  LED_LOCK = 11,  ///< Lock indicator
  LED_S2   = 12,  ///< Mode indicator S2
  LED_S1   = 13,  ///< Mode indicator S1
  LED15 = 14,  // GPS bar segment 1 (lowest)
  LED16 = 15,  // GPS bar segment 2
  LED17 = 16,  // GPS bar segment 3
  LED18 = 17,  // GPS bar segment 4 (highest)
};

// ---- LED states (3 bits per LED, packed into 64-bit leds field) ----

enum LED_state : uint8_t {
  LED_off        = 0b000,
  // 0b001..0b100 reserved
  LED_blink_slow = 0b101,
  LED_blink_fast = 0b110,
  LED_on         = 0b111,
};

// ---- Emergency bitmask (msg_event_emergency.state) ----

enum Emergency_state : uint8_t {
  Emergency_latch = 0b00001,  ///< Latched emergency
  Emergency_stop1 = 0b00010,  ///< Stop button 1
  Emergency_stop2 = 0b00100,  ///< Stop button 2
  Emergency_lift1 = 0b01000,  ///< Lift sensor / wheel lifted
  Emergency_lift2 = 0b10000,  ///< Lift sensor 2 / extended lift detection
};

// ---- Topic subscription bitmask (msg_event_subscribe.topic_bitmask) ----

enum Topic_state : uint8_t {
  Topic_set_leds      = 1 << 0,  ///< Subscribe to LED update messages
  Topic_set_ll_status = 1 << 1,  ///< Subscribe to low-level status messages
  Topic_set_hl_state  = 1 << 2,  ///< Subscribe to high-level state messages
};

// ---- Message structures (packed, CRC-16 over all bytes except the crc field itself) ----

#pragma pack(push, 1)

struct msg_get_version {
  uint8_t  type;      ///< YFCoverUIType::Get_Version
  uint8_t  reserved;  ///< Padding
  uint16_t version;   ///< Firmware version
  uint16_t crc;       ///< CRC-16/CCITT
};

struct msg_set_buzzer {
  uint8_t type;      ///< YFCoverUIType::Set_Buzzer
  uint8_t repeat;    ///< Repeat count
  uint8_t on_time;   ///< Signal on time (units defined by UI firmware)
  uint8_t off_time;  ///< Signal off time
  uint16_t crc;      ///< CRC-16/CCITT
};

/**
 * @brief LED matrix update.
 *
 * Each of the 18 LEDs occupies 3 bits in the `leds` field:
 *   bits [2:0]  = LED 0 (LED_CHARGING)
 *   bits [5:3]  = LED 1 (LED_BATTERY_LOW)
 *   ...
 *   bits [53:51] = LED 17 (LED18)
 *
 * See LED_state for bit encoding.
 */
struct msg_set_leds {
  uint8_t  type;      ///< YFCoverUIType::Set_LEDs
  uint8_t  reserved;  ///< Padding
  uint64_t leds;      ///< 18 × 3 bits of LED states
  uint16_t crc;       ///< CRC-16/CCITT
};

struct msg_event_button {
  uint8_t  type;            ///< YFCoverUIType::Get_Button
  uint16_t button_id;       ///< Button identifier
  uint8_t  press_duration;  ///< 0=single, 1=long, 2=very long
  uint16_t crc;             ///< CRC-16/CCITT
};

struct msg_event_emergency {
  uint8_t type;   ///< YFCoverUIType::Get_Emergency
  uint8_t state;  ///< Bitmask, see Emergency_state
  uint16_t crc;   ///< CRC-16/CCITT
};

struct msg_event_rain {
  uint8_t  type;       ///< YFCoverUIType::Get_Rain
  uint8_t  reserved;   ///< Padding
  uint32_t value;      ///< Raw rain sensor ADC value
  uint32_t threshold;  ///< Rain threshold (value < threshold → raining)
  uint16_t crc;        ///< CRC-16/CCITT
};

struct msg_event_subscribe {
  uint8_t  type;          ///< YFCoverUIType::Get_Subscribe
  uint8_t  topic_bitmask; ///< Bitmask of topics to receive, see Topic_state
  uint16_t interval;      ///< Update interval in ms
  uint16_t crc;           ///< CRC-16/CCITT
};

#pragma pack(pop)

// ---- LED helper functions ----

/**
 * @brief Set a single LED state in the leds bitmask.
 * @param msg   The Set_LEDs message to modify.
 * @param led   LED index (0–17), use LED_id enum.
 * @param state LED state, use LED_state enum.
 */
inline void setLed(struct msg_set_leds& msg, int led, uint8_t state) {
  uint64_t mask = ~(static_cast<uint64_t>(0b111) << (led * 3));
  msg.leds &= mask;
  msg.leds |= static_cast<uint64_t>(state & 0b111) << (led * 3);
}

/**
 * @brief Set the 7-segment battery bar LEDs (LED5–LED11) based on a 0.0–1.0 value.
 * @param msg   The Set_LEDs message to modify.
 * @param value Battery fraction, 0.0 = empty, 1.0 = full.
 */
inline void setBars7(struct msg_set_leds& msg, double value) {
  int on_leds = static_cast<int>(round(value * 7.0));
  for (int i = 0; i < 7; i++) {
    setLed(msg, LED11 - i, i < on_leds ? LED_on : LED_off);
  }
}

/**
 * @brief Set the 4-segment GPS bar LEDs (LED15–LED18) based on a 0.0–1.0 value.
 * @param msg   The Set_LEDs message to modify.
 * @param value GPS quality fraction, 0.0–1.0. Use a negative value to fast-blink all.
 */
inline void setBars4(struct msg_set_leds& msg, double value) {
  if (value < 0.0) {
    for (int i = 0; i < 4; i++) {
      setLed(msg, LED15 + i, LED_blink_fast);
    }
  } else {
    int on_leds = static_cast<int>(round(value * 4.0));
    for (int i = 0; i < 4; i++) {
      setLed(msg, LED18 - i, i < on_leds ? LED_on : LED_off);
    }
  }
}

#endif  // YF_COVER_UI_PROTOCOL_HPP
