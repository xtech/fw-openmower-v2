#ifndef _UI_DATATYPES_H_
#define _UI_DATATYPES_H_

#include <stdint.h>

#include <cmath>
// Protocol Header Info
enum TYPE {
  Get_Version = 0xB0,
  Set_Buzzer = 0xB1,
  Set_LEDs = 0xB2,
  Get_Button = 0xB3,
  Get_Emergency = 0xB4,  // Stock-CoverUI
  Get_Rain = 0xB5,       // Stock-CoverUI
  Get_Subscribe = 0xB6
};

// Function definitions for the 18 LEDS
enum LED_id {
  LED_CHARGING = 0,
  LED_BATTERY_LOW = 1,
  LED_POOR_GPS = 2,
  LED_MOWER_LIFTED = 3,
  LED5 = 4,
  LED6 = 5,
  LED7 = 6,
  LED8 = 7,
  LED9 = 8,
  LED10 = 9,
  LED11 = 10,
  LED_LOCK = 11,
  LED_S2 = 12,
  LED_S1 = 13,
  LED15 = 14,
  LED16 = 15,
  LED17 = 16,
  LED18 = 17
};

enum LED_state { LED_off = 0b000, LED_blink_slow = 0b101, LED_blink_fast = 0b110, LED_on = 0b111 };

// Stock-CoverUI (same bitmask as in ll_status.emergency_bitmask of datatypes.h)
enum Emergency_state {
  Emergency_latch = 0b00001,
  Emergency_stop1 = 0b00010,
  Emergency_stop2 = 0b00100,
  Emergency_lift1 = 0b01000,
  Emergency_lift2 = 0b10000
};

enum Topic_state {
  Topic_set_leds = 1 << 0,
  Topic_set_ll_status = 1 << 1,
  Topic_set_hl_state = 1 << 2,
};

#pragma pack(push, 1)
struct msg_get_version {
  uint8_t type;      // command type
  uint8_t reserved;  // padding
  uint16_t version;
  uint16_t crc;  // CRC 16
} __attribute__((packed));
#pragma pack(pop)

#pragma pack(push, 1)
struct msg_set_buzzer {
  uint8_t type;      // command type
  uint8_t repeat;    // Repeat X times
  uint8_t on_time;   // Signal on time
  uint8_t off_time;  // Signal off time
  uint16_t crc;      // CRC 16
} __attribute__((packed));
#pragma pack(pop)

/**
 * @brief Use this to update the LED matrix
 * Each LED gets three bits with the following meaning:
 * 0b000 = Off
 * 0b001 = reserved for future use
 * 0b010 = reserved for future use
 * 0b011 = reserved for future use
 * 0b100 = reserved for future use
 * 0b101 = On slow blink
 * 0b110 = On fast blink
 * 0b111 = On
 */
#pragma pack(push, 1)
struct msg_set_leds {
  uint8_t type;      // command type
  uint8_t reserved;  // padding
  uint64_t leds;
  uint16_t crc;  // CRC 16
} __attribute__((packed));
#pragma pack(pop)

#pragma pack(push, 1)
struct msg_event_button {
  uint8_t type;  // command type
  uint16_t button_id;
  uint8_t press_duration;
  uint16_t crc;  // CRC 16
} __attribute__((packed));
#pragma pack(pop)

// Stock-CoverUI
#pragma pack(push, 1)
struct msg_event_rain {
  uint8_t type;      // Command type
  uint8_t reserved;  // Padding
  uint32_t value;
  uint32_t threshold;  // If value < threshold then it rains. Why a threshold? Cause there might be a future option to
                       // make it configurable on Stock-CoverUI
  uint16_t crc;        // CRC 16
} __attribute__((packed));
#pragma pack(pop)

// Stock-CoverUI
#pragma pack(push, 1)
struct msg_event_emergency {
  uint8_t type;   // Command type
  uint8_t state;  // Same as in ll_status.emergency_bitmask of datatypes.h
  uint16_t crc;   // CRC 16
} __attribute__((packed));
#pragma pack(pop)

// Cover UI might subscribe in what data it's interested to receive

#pragma pack(push, 1)
struct msg_event_subscribe {
  uint8_t type;           // Command type
  uint8_t topic_bitmask;  // Bitmask of data subscription(s), see Topic_state
  uint16_t interval;      // Interval (ms) how often to send topic(s)
  uint16_t crc;           // CRC 16
} __attribute__((packed));
#pragma pack(pop)

inline void setLed(struct msg_set_leds &msg, int led, uint8_t state) {
  // mask the current led state
  uint64_t mask = ~(((uint64_t)0b111) << (led * 3));
  msg.leds &= mask;
  msg.leds |= ((uint64_t)(state & 0b111)) << (led * 3);
}

inline void setBars7(struct msg_set_leds &msg, double value) {
  int on_leds = round(value * 7.0);
  for (int i = 0; i < 7; i++) {
    setLed(msg, LED11 - i, i < on_leds ? LED_on : LED_off);
  }
}

inline void setBars4(struct msg_set_leds &msg, double value) {
  if (value < 0) {
    for (int i = 0; i < 4; i++) {
      setLed(msg, i + LED15, LED_blink_fast);
    }
  } else {
    int on_leds = round(value * 4.0);
    for (int i = 0; i < 4; i++) {
      setLed(msg, LED18 - i, i < on_leds ? LED_on : LED_off);
    }
  }
}

#endif  // _UI_DATATYPES_H_
