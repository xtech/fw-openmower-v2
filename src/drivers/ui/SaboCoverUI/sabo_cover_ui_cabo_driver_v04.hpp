/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sabo_cover_ui_cabo_driver_v04.hpp
 * @brief Sabo CoverUI Cabo Driver for Hardware v0.4 with one TCA9535 GPIO expander
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-05-03
 */

#ifndef OPENMOWER_SABO_COVER_UI_CABO_DRIVER_V04_HPP
#define OPENMOWER_SABO_COVER_UI_CABO_DRIVER_V04_HPP

#include <ulog.h>

#include <cassert>

#include "drivers/gpio/tca_95xx/tca9535.hpp"
#include "sabo_cover_ui_cabo_driver_base.hpp"
#include "sabo_cover_ui_series1_v04.hpp"
#include "sabo_cover_ui_series2.hpp"

namespace xbot::driver::ui {

using namespace xbot::driver::sabo::types;
using namespace xbot::driver::gpio;

// Sabo CoverUI Driver for Hardware v0.4
class SaboCoverUICaboDriverV04 : public SaboCoverUICaboDriverBase {
 public:
  explicit SaboCoverUICaboDriverV04(const xbot::driver::sabo::config::CoverUi* cover_ui_cfg)
      : SaboCoverUICaboDriverBase(cover_ui_cfg),
        gpio_exp_(&cover_ui_cfg->gpio_expander.v04.expander),
        pins_(cover_ui_cfg_->pins.v04) {
  }

  bool Init() override {
    if (!SaboCoverUICaboDriverBase::Init()) return false;

    // Set HEF4794BT STR (LEDs and button row) to low by default before Series detection / initialization
    palSetLineMode(pins_.s2_latch, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_PULLUP);
    palWriteLine(pins_.s2_latch, PAL_LOW);

    // Set S2-Load to low to block HC165 shifting
    palSetLineMode(pins_.s2_load, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_PULLUP);
    palWriteLine(pins_.s2_load, PAL_LOW);

    // Set S1-/CON and S2-/CON to input with pull-up
    palSetLineMode(pins_.s1_con, PAL_MODE_INPUT_PULLUP);
    palSetLineMode(pins_.s2_con, PAL_MODE_INPUT_PULLUP);

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

  // Latch LEDs (as well as button-row if exists) and load button columns
  void LatchLoad() override {
    uint8_t btn_cols;

    if (!series_) return;

    switch (series_->GetType()) {
      case SeriesType::Series1:
        gpio_exp_.WritePort(~current_led_mask_);  // Series-I LEDs are active low, so invert the mask

        // Buttons
        uint16_t btns;
        gpio_exp_.ReadPort(&btns);
        btns &= SaboCoverUISeries1V04::GPIO_BTNS_MASK_BTN_ALL_L;   // Mask out buttons
        btns |= ~SaboCoverUISeries1V04::GPIO_BTNS_MASK_BTN_ALL_L;  // Set non-button bits to 1 for easier diagnostics
        DebounceRawButtons(btns);
        break;
      case SeriesType::Series2:
        btn_cols = Series2LatchLoad(current_led_mask_ |
                                    series_->GetButtonRowMask());  // Latch LEDs and button row, return button columns
        DebounceRawButtons(series_->ProcessButtonCol(btn_cols));   // Process received button column
        break;
      default: ULOG_ERROR("Unknown Sabo CoverUI series type"); return;
    }
  };

 protected:
  TCA9535Driver gpio_exp_;  // GPIO expander for Series-I buttons and LEDs

  uint16_t MapLedIdToMask(LedId id) const override {
    return series_ ? series_->MapLedIdToMask(id) : 0;
  };

  /**
   * @brief Send tx_data to Series-II (HEF4794) "Shift and Store Register" and latch it.
   *
   * @param tx_data
   */
  uint8_t Series2LatchLoad(uint8_t tx_data) {
    uint8_t rx_data = 0;

    spiAcquireBus(cover_ui_cfg_->spi.instance);

    spiStart(cover_ui_cfg_->spi.instance, &spi_config_);

    palWriteLine(pins_.s2_load, PAL_HIGH);  // Set S2-Load to high to enable HC165 shifting

    // Send LEDs (& button rows) and read button columns
    spiExchange(cover_ui_cfg_->spi.instance, 1, &tx_data, &rx_data);

    palWriteLine(pins_.s2_latch, PAL_HIGH);  // Latch HEF4794
    palWriteLine(pins_.s2_load, PAL_LOW);    // Set S2-Load to low to block HC165 shifting
    palWriteLine(pins_.s2_latch, PAL_LOW);   // Close HEF4794 latch

    spiReleaseBus(cover_ui_cfg_->spi.instance);

    return rx_data;
  }

  SaboCoverUISeriesInterface* GetSeriesDriver() override {
    static systime_t next_process_time = 0;

    if (chVTGetSystemTimeX() < next_process_time) {
      return nullptr;
    }
    next_process_time = chVTGetSystemTimeX() + TIME_S2I(2);  // Process only every 2nd second

    // Series-I CoverUI has it's /Connected signal on GPIO port
    if (palReadLine(pins_.s1_con) == PAL_LOW) {
      ULOG_INFO("Detected Sabo Series-I CoverUI");
      static SaboCoverUISeries1V04 series1_driver;
      return &series1_driver;
    }

    // Series-II CoverUI has it's /Connected signal on GPIO port
    if (palReadLine(pins_.s2_con) == PAL_LOW) {
      ULOG_INFO("Detected Sabo Series-II CoverUI");

      Series2LatchLoad(0);  // Latch LEDs=off and select no button row

      static SaboCoverUISeries2 series2_driver;
      return &series2_driver;
    }

    ULOG_INFO("No Sabo CoverUI detected");
    return nullptr;
  }

 private:
  const decltype(xbot::driver::sabo::config::CoverUi::pins.v04)& pins_;
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_CABO_DRIVER_V04_HPP
