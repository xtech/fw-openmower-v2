/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sabo_cover_ui_cabo_driver_v05.hpp
 * @brief Sabo CoverUI Cabo Driver for Hardware v0.5 with TCA9535 CoverUI Series detection
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-06-14
 */

#ifndef OPENMOWER_SABO_COVER_UI_CABO_DRIVER_V05_HPP
#define OPENMOWER_SABO_COVER_UI_CABO_DRIVER_V05_HPP

#include <ulog.h>

#include "sabo_cover_ui_cabo_driver_v04.hpp"
#include "sabo_cover_ui_series1_v04.hpp"
#include "sabo_cover_ui_series2.hpp"

namespace xbot::driver::ui {

using namespace xbot::driver::sabo::types;
using namespace xbot::driver::gpio;

// Sabo CoverUI Driver for Hardware v0.5
// Differs from v0.4 only in CoverUI Series detection: uses TCA9535 bits instead of dedicated GPIO pins
class SaboCoverUICaboDriverV05 : public SaboCoverUICaboDriverV04 {
 public:
  explicit SaboCoverUICaboDriverV05(const xbot::driver::sabo::config::CoverUi* cover_ui_cfg)
      : SaboCoverUICaboDriverV04(cover_ui_cfg), pins_(cover_ui_cfg_->pins.v05) {
  }

  bool Init() override {
    if (!SaboCoverUICaboDriverBase::Init()) return false;

    // Set HEF4794BT STR (LEDs and button row) to low by default before Series detection / initialization
    palSetLineMode(pins_.s2_latch, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_PULLUP);
    palWriteLine(pins_.s2_latch, PAL_LOW);

    // Set S2-Load to low to block HC165 shifting
    palSetLineMode(pins_.s2_load, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_PULLUP);
    palWriteLine(pins_.s2_load, PAL_LOW);

    // Initialize GPIO expander
    if (!gpio_exp_.Init()) {
      return false;
    }

    // Configure GPIO expander for Series-I LEDs
    if (!gpio_exp_.ConfigPort(
            ~SaboCoverUISeries1V04::GPIO_LED_PINS)) {  // 0 = output, rest = inputs (buttons and TestPoints)
      return false;
    }

    return true;
  }

 protected:
  SaboCoverUISeriesInterface* GetSeriesDriver() override {
    static systime_t next_process_time = 0;

    if (chVTGetSystemTimeX() < next_process_time) {
      return nullptr;
    }
    next_process_time = chVTGetSystemTimeX() + TIME_S2I(2);  // Process only every 2nd second

    uint16_t gpio_port_data = 0;

    if (!gpio_exp_.ReadPort(&gpio_port_data)) {
      ULOG_ERROR("Failed to read from GPIO expander, cannot detect Sabo CoverUI");
      return nullptr;
    }

    // Series-I CoverUI has its /Connected signal on TCA9535 bit 14
    if ((gpio_port_data & pins_.tca_s1_con_mask) == 0) {
      ULOG_INFO("Detected Sabo Series-I CoverUI");
      static SaboCoverUISeries1V04 series1_driver;
      return &series1_driver;
    }

    // Series-II CoverUI has its /Connected signal on TCA9535 bit 15
    if ((gpio_port_data & pins_.tca_s2_con_mask) == 0) {
      ULOG_INFO("Detected Sabo Series-II CoverUI");

      Series2LatchLoad(0);  // Latch LEDs=off and select no button row

      static SaboCoverUISeries2 series2_driver;
      return &series2_driver;
    }

    ULOG_INFO("No Sabo CoverUI detected");
    return nullptr;
  }

 private:
  const decltype(xbot::driver::sabo::config::CoverUi::pins.v05)& pins_;
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_CABO_DRIVER_V05_HPP
