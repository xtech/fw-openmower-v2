/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sabo_cover_ui_defs.hpp
 * @brief Definitions for the Sabo Cover UI
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-05-31
 */

#ifndef OPENMOWER_SABO_COVER_UI_DEFS_HPP
#define OPENMOWER_SABO_COVER_UI_DEFS_HPP

#include "../../../filesystem/versioned_struct.hpp"
#include "ch.h"

namespace xbot::driver::ui::sabo {

struct SPICfg {
  SPIDriver* instance = nullptr;  // SPI-Instance like &SPID1
  struct {
    ioline_t sck = PAL_NOLINE;   // Clock (SCK/CLK)
    ioline_t miso = PAL_NOLINE;  // Master In Slave Out (MISO/RX)
    ioline_t mosi = PAL_NOLINE;  // Master Out Slave In (MOSI/TX)
    ioline_t cs = PAL_NOLINE;    // Chip Select (/CS)
  } pins;
};

struct CaboCfg {
  SPICfg spi;  // SPI configuration for the LCD
               // Shift Register Pins for HC165 parallel load and HC595 or HEF4794BT latch/load
  struct {
    ioline_t latch_load;
    ioline_t oe = PAL_NOLINE;
    ioline_t btn_cs = PAL_NOLINE;  // Button Chip Select (74HC165 /CE) only avail for Series-II CoverUI
    ioline_t inp_cs = PAL_NOLINE;  // Input Chip Select (74HC165 /CE) only avail in HW >= v0.2
  } pins;
};

struct LCDCfg {
  SPICfg spi;  // SPI configuration for the LCD
  struct {
    ioline_t dc = PAL_NOLINE;   // Data/Command (D/C)
    ioline_t rst = PAL_NOLINE;  // Reset (/RST)
    ioline_t backlight = PAL_NOLINE;
  } pins;
};

struct CoverUICfg {
  CaboCfg cabo_cfg;  // SPI and control pins for the CoverUI (LEDs, Buttons, Display
  LCDCfg lcd_cfg;    // SPI and control pins for the CoverUI LCD (Chip Select, Data/Command, Reset)
};

// Same bit order as in CoverUI Series-II (HEF4794BT). Series-I driver need to map the bits
enum class LEDID : uint8_t { AUTO = 0, MOWING, HOME, PLAY_GN, PLAY_RD };
enum class LEDMode { OFF, ON, BLINK_SLOW, BLINK_FAST };

enum class SeriesType { Series1, Series2 };

// Same bit order as in CoverUI Series-II (74HC165, Row 0+1). Series-I driver need to translate the bits
enum class ButtonID {
  UP = 0,
  DOWN,
  LEFT,
  RIGHT,
  OK,
  PLAY,
  S1_SELECT,
  MENU = 8,
  BACK,
  S2_AUTO,
  S2_MOW,
  S2_HOME,
  _FIRST = UP,
  _LAST = S2_HOME
};

/**
 * @brief Convert ButtonID to human-readable string
 */
inline const char* ButtonIDToString(ButtonID id) {
  switch (id) {
    case ButtonID::UP: return "Up";
    case ButtonID::DOWN: return "Down";
    case ButtonID::LEFT: return "Left";
    case ButtonID::RIGHT: return "Right";
    case ButtonID::OK: return "OK";
    case ButtonID::PLAY: return "Play";
    case ButtonID::S1_SELECT: return "Select (S1)";
    case ButtonID::MENU: return "Menu";
    case ButtonID::BACK: return "Back";
    case ButtonID::S2_AUTO: return "Auto (S2)";
    case ButtonID::S2_MOW: return "Mow (S2)";
    case ButtonID::S2_HOME: return "Home (S2)";
    default: return "Unknown";
  }
}

namespace display {
constexpr uint16_t LCD_WIDTH = 240;  // ATTENTION: LVGL I1 mode requires a multiple of 8 width
constexpr uint16_t LCD_HEIGHT = 160;
constexpr uint8_t BUFFER_FRACTION = 10;  // 1/10 screen size for buffers
}  // namespace display

namespace settings {

/**
 * @brief LCD Temperature Compensation modes
 * Maps directly to UC1698 hardware register values
 */
enum class TempCompensation : uint8_t {
  OFF = 0,     // No temperature compensation
  LOW = 1,     // -0.05%/°C
  MEDIUM = 2,  // -0.15%/°C (recommended default)
  HIGH = 3     // -0.25%/°C
};

/**
 * @brief LCD persistent settings stored in LittleFS
 *
 * This struct is serialized directly to flash as binary data.
 * Evolution strategy: version field + append-only new fields.
 *
 * Rules for evolution:
 * - NEVER change existing field types or order
 * - ALWAYS increment VERSION when adding fields
 * - ONLY append new fields at the end
 * - Use padding to maintain alignment if needed
 *
 * Migration is handled automatically by VersionedStruct base class.
 */
#pragma pack(push, 1)
struct LCDSettings : public xbot::driver::filesystem::VersionedStruct<xbot::driver::ui::sabo::settings::LCDSettings> {
  VERSIONED_STRUCT_FIELDS(1);  // Version 1 - automatically defines VERSION constant and version field
  static constexpr const char* PATH = "/cfg/sabo/lcd.bin";

  uint8_t contrast = 100;                                         // LCD contrast (0-255)
  TempCompensation temp_compensation = TempCompensation::MEDIUM;  // Temperature compensation
  uint8_t auto_sleep_minutes = 5;                                 // Auto-sleep timeout (1-20 minutes)
};
#pragma pack(pop)

static_assert(sizeof(LCDSettings) == 5,
              "LCDSettings must be 5 bytes (2 version + 3 data)");  // Protect against thoughtless changes

}  // namespace settings

}  // namespace xbot::driver::ui::sabo

#endif  // OPENMOWER_SABO_COVER_UI_DEFS_HPP
