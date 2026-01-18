/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sabo_common.hpp
 * @brief Common definitions and types for Sabo robot platform
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-11-13
 */

#ifndef SABO_COMMON_HPP
#define SABO_COMMON_HPP

#include <etl/array_view.h>
#include <hal.h>

#include <cstdint>
#include <globals.hpp>

#include "../../src/filesystem/versioned_struct.hpp"
#include "GpsServiceBase.hpp"

namespace xbot::driver::sabo {

// Main types and enums
namespace types {
// Hardware versions are "as of" versions
enum class HardwareVersion : uint8_t { V0_1 = 0, V0_2, V0_3 };

enum class SeriesType { Series1, Series2 };

enum class InputType : uint8_t { SENSOR, BUTTON };

enum class SensorId : uint8_t { LIFT_FL, LIFT_FR, STOP_TOP, STOP_REAR };

enum class ScreenId : uint8_t { BOOT, MAIN, MENU, CMD, SETTINGS, BATTERY, INPUTS, ABOUT };

enum class ButtonId : uint8_t {
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
  S2_HOME
};

enum class LedId : uint8_t { AUTO = 0, MOWING, HOME, PLAY_GN, PLAY_RD };

enum class LedMode : uint8_t { OFF, ON, BLINK_SLOW, BLINK_FAST };

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
}  // namespace types

// Hardware configurations
namespace config {
struct Sensor {
  ioline_t line;
  bool invert = false;
};

struct Spi {
  SPIDriver* instance;
  struct {
    ioline_t sck;
    ioline_t miso;
    ioline_t mosi;
    ioline_t cs = PAL_NOLINE;
  } pins;
};

struct CoverUi {
  Spi spi;
  struct {
    ioline_t latch_load;
    ioline_t oe = PAL_NOLINE;
    ioline_t btn_cs = PAL_NOLINE;
    ioline_t inp_cs = PAL_NOLINE;
  } pins;
};

struct Lcd {
  Spi spi;
  struct {
    ioline_t dc;
    ioline_t rst;
    ioline_t backlight;
  } pins;
};

struct Bms {
  I2CDriver* i2c;
};

// Individual hardware configurations
inline const Sensor SENSORS_V0_1[] = {
    {LINE_GPIO13},  // LIFT_FL
    {LINE_GPIO12},  // LIFT_FR
    {LINE_GPIO11},  // STOP_TOP
};

inline const Sensor SENSORS_V0_3[] = {
    {LINE_GPIO13, true},  // LIFT_FL
    {LINE_GPIO12, true},  // LIFT_FR
    {LINE_GPIO11, true},  // STOP_TOP
};

#ifndef STM32_SPI_USE_SPI1
#error STM32_SPI_USE_SPI1 must be enabled for CoverUI support
#endif

inline const CoverUi COVER_UI_V0_1 = {.spi = {&SPID1, {LINE_SPI1_SCK, LINE_SPI1_MISO, LINE_SPI1_MOSI, PAL_NOLINE}},
                                      .pins = {LINE_GPIO9, LINE_GPIO8, LINE_GPIO1, PAL_NOLINE}};

inline const CoverUi COVER_UI_V0_2 = {.spi = {&SPID1, {LINE_SPI1_SCK, LINE_SPI1_MISO, LINE_SPI1_MOSI, PAL_NOLINE}},
                                      .pins = {LINE_GPIO9, PAL_NOLINE, PAL_NOLINE, LINE_GPIO1}};

inline const Lcd LCD_V0_2 = {.spi = {&SPID1, {LINE_SPI1_SCK, LINE_SPI1_MISO, LINE_SPI1_MOSI, LINE_GPIO5}},
                             .pins = {LINE_AGPIO4, LINE_UART7_TX, LINE_GPIO3}};

#ifndef STM32_SPI_USE_SPI2
#error STM32_SPI_USE_SPI2 must be enabled for BMS support
#endif

inline const Bms BMS_V0_2 = {.i2c = &I2CD2};

// Hardware configuration references
struct HardwareConfig {
  etl::array_view<const Sensor> sensors;
  const CoverUi* cover_ui;
  const Lcd* lcd;
  const Bms* bms;
};

// Hardware version to configuration array which need to be in sync with HardwareVersion enum
inline constexpr HardwareConfig HARDWARE_CONFIGS[] = {
    // V0_1
    {
        .sensors = etl::array_view<const Sensor>(SENSORS_V0_1),
        .cover_ui = &COVER_UI_V0_1,
        .lcd = nullptr,  // No LCD for V0_1
        .bms = nullptr,  // No BMS for V0_1
    },
    // V0_2
    {
        .sensors = etl::array_view<const Sensor>(SENSORS_V0_1),
        .cover_ui = &COVER_UI_V0_2,
        .lcd = &LCD_V0_2,
        .bms = &BMS_V0_2,
    },
    // V0_3
    {
        .sensors = etl::array_view<const Sensor>(SENSORS_V0_3),
        .cover_ui = &COVER_UI_V0_2,
        .lcd = &LCD_V0_2,
        .bms = nullptr,  // No BMS for V0_3 yet
    },
};
}  // namespace config

// Constants and definitions
namespace defs {
inline constexpr types::ButtonId ALL_BUTTONS[] = {
    types::ButtonId::UP,   types::ButtonId::DOWN,    types::ButtonId::LEFT,      types::ButtonId::RIGHT,
    types::ButtonId::OK,   types::ButtonId::PLAY,    types::ButtonId::S1_SELECT, types::ButtonId::MENU,
    types::ButtonId::BACK, types::ButtonId::S2_AUTO, types::ButtonId::S2_MOW,    types::ButtonId::S2_HOME};

inline constexpr size_t NUM_BUTTONS = sizeof(ALL_BUTTONS) / sizeof(ALL_BUTTONS[0]);

// Display constants
inline constexpr uint16_t LCD_WIDTH = 240;  // ATTENTION: LVGL I1 mode requires a multiple of 8 width
inline constexpr uint16_t LCD_HEIGHT = 160;
inline constexpr uint8_t BUFFER_FRACTION = 10;  // 1/10 screen size for buffers

// INA180A1 input current sensor constants
inline constexpr float ICS_SHUNT_RESISTANCE = 0.1f;                // 0.1 Ohm
inline constexpr float ICS_VOLTAGE_DIVIDER_RATIO = 27.0f / 49.0f;  // 27k / (22k + 27k)
inline constexpr float ICS_GAIN = 20.0f;                           // V/V for INA180A1
inline constexpr float ADC_REF_VOLTAGE = 3.3f;                     // V
inline constexpr uint32_t ADC_MAX_VALUE = 4095;                    // 12-bit ADC
inline const ioline_t ICS_GPIO = LINE_AGPIO2;                      // PA4
}  // namespace defs

// Settings namespace for persistent configuration
namespace settings {

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
struct LCDSettings : public xbot::driver::filesystem::VersionedStruct<LCDSettings> {
  VERSIONED_STRUCT_FIELDS(1);  // Version 1 - automatically defines VERSION constant and version field
  static constexpr const char* PATH = "/cfg/sabo/lcd.bin";

  uint8_t contrast = 80;                                                        // LCD contrast (0-255)
  types::TempCompensation temp_compensation = types::TempCompensation::MEDIUM;  // Temperature compensation
  uint8_t auto_sleep_minutes = 5;                                               // Auto-sleep timeout (1-20 minutes)
};
#pragma pack(pop)

// Protect against thoughtless changes
static_assert(sizeof(LCDSettings) == 5, "LCDSettings must be 5 bytes (2 version + 3 data)");

/**
 * @brief GPS persistent settings stored in LittleFS
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
struct GPSSettings : public xbot::driver::filesystem::VersionedStruct<GPSSettings> {
  VERSIONED_STRUCT_FIELDS(1);  // Version 1 - automatically defines VERSION constant and version field
  static constexpr const char* PATH = "/cfg/sabo/gps.bin";

  ProtocolType protocol = ProtocolType::NMEA;  // GPS protocol (UBX or NMEA)
  uint8_t uart = 0;                            // UART index (0 = default robot port)
  uint32_t baudrate = 921600;                  // Baudrate
};
#pragma pack(pop)

// Protect against thoughtless changes
static_assert(sizeof(GPSSettings) == 8, "GPSSettings must be 8 bytes (2 version + 1 protocol + 1 uart + 4 baudrate)");

}  // namespace settings

// Compile-time validation for supported hardware versions
static_assert(static_cast<uint8_t>(types::HardwareVersion::V0_3) + 1 ==
                  sizeof(config::HARDWARE_CONFIGS) / sizeof(config::HARDWARE_CONFIGS[0]),
              "HardwareVersion enum count must match HARDWARE_CONFIGS array size");

inline types::HardwareVersion GetHardwareVersion(const struct carrier_board_info& board_info) {
  types::HardwareVersion version = types::HardwareVersion::V0_3;  // Default fallback

  if (board_info.version_major == 0) {
    switch (board_info.version_minor) {
      case 1: version = types::HardwareVersion::V0_1; break;
      case 2: version = types::HardwareVersion::V0_2; break;
      case 3: version = types::HardwareVersion::V0_3; break;
    }
  }
  return version;
}

// Factory function to get hardware configuration based on version
inline const config::HardwareConfig& GetHardwareConfig(types::HardwareVersion version) {
  const auto index = static_cast<uint8_t>(version);
  return config::HARDWARE_CONFIGS[index];
}

/**
 * @brief Convert ButtonID to human-readable string
 */
inline const char* ButtonIdToString(types::ButtonId id) {
  switch (id) {
    case types::ButtonId::UP: return "Up";
    case types::ButtonId::DOWN: return "Down";
    case types::ButtonId::LEFT: return "Left";
    case types::ButtonId::RIGHT: return "Right";
    case types::ButtonId::OK: return "OK";
    case types::ButtonId::PLAY: return "Play";
    case types::ButtonId::S1_SELECT: return "Select (S1)";
    case types::ButtonId::MENU: return "Menu";
    case types::ButtonId::BACK: return "Back";
    case types::ButtonId::S2_AUTO: return "Auto (S2)";
    case types::ButtonId::S2_MOW: return "Mow (S2)";
    case types::ButtonId::S2_HOME: return "Home (S2)";
    default: return "Unknown";
  }
}

}  // namespace xbot::driver::sabo

#endif  // SABO_COMMON_HPP
