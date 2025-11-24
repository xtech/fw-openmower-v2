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

namespace xbot::driver::sabo {

// Main types and enums
namespace types {
// Hardware versions are "as of" versions
enum class HardwareVersion : uint8_t { V0_1 = 0, V0_2, V0_3 };

enum class InputType : uint8_t { SENSOR, BUTTON };

enum class SensorId : uint8_t { LIFT_FL, LIFT_FR, STOP_TOP, STOP_REAR };

enum class ScreenId : uint8_t { BOOT, MAIN, MENU, CMD, INPUTS, SETTINGS, ABOUT };

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

inline const CoverUi COVER_UI_V0_1 = {.spi = {&SPID1, {LINE_SPI1_SCK, LINE_SPI1_MISO, LINE_SPI1_MOSI, PAL_NOLINE}},
                                      .pins = {LINE_GPIO9, LINE_GPIO8, LINE_GPIO1, PAL_NOLINE}};

inline const CoverUi COVER_UI_V0_2 = {.spi = {&SPID1, {LINE_SPI1_SCK, LINE_SPI1_MISO, LINE_SPI1_MOSI, PAL_NOLINE}},
                                      .pins = {LINE_GPIO9, PAL_NOLINE, PAL_NOLINE, LINE_GPIO1}};

inline const Lcd LCD_V0_2 = {.spi = {&SPID1, {LINE_SPI1_SCK, LINE_SPI1_MISO, LINE_SPI1_MOSI, LINE_GPIO5}},
                             .pins = {LINE_AGPIO4, LINE_UART7_TX, LINE_GPIO3}};

// Hardware configuration references
struct HardwareConfig {
  etl::array_view<const Sensor> sensors;
  const CoverUi* cover_ui;
  const Lcd* lcd;
};

// Hardware version to configuration array which need to be in sync with HardwareVersion enum
inline constexpr HardwareConfig HARDWARE_CONFIGS[] = {
    // V0_1
    {
        .sensors = etl::array_view<const Sensor>(SENSORS_V0_1),
        .cover_ui = &COVER_UI_V0_1,
        .lcd = nullptr,  // No LCD for V0_1
    },
    // V0_2
    {
        .sensors = etl::array_view<const Sensor>(SENSORS_V0_1),
        .cover_ui = &COVER_UI_V0_2,
        .lcd = &LCD_V0_2,
    },
    // V0_3
    {
        .sensors = etl::array_view<const Sensor>(SENSORS_V0_3),
        .cover_ui = &COVER_UI_V0_2,
        .lcd = &LCD_V0_2,
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
}  // namespace defs

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

}  // namespace xbot::driver::sabo

#endif  // SABO_COMMON_HPP
