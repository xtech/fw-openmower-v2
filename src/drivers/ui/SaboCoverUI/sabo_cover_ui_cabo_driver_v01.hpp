/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/**
 * @file sabo_cover_ui_cabo_driver_v01.hpp
 * @brief Sabo CoverUI Cabo Driver for Hardware v0.1 with direct Series-II HEF4794BT/HC165 support
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-06-01
 */

#ifndef OPENMOWER_SABO_COVER_UI_CABO_DRIVER_V01_HPP
#define OPENMOWER_SABO_COVER_UI_CABO_DRIVER_V01_HPP

#include "sabo_cover_ui_cabo_driver_base.hpp"
#include "sabo_cover_ui_series2.hpp"

namespace xbot::driver::ui {

using namespace xbot::driver::sabo::types;

// Sabo CoverUI Driver for Hardware v0.1
class SaboCoverUICaboDriverV01 : public SaboCoverUICaboDriverBase {
 public:
  explicit SaboCoverUICaboDriverV01(const xbot::driver::sabo::config::CoverUi* cover_ui_cfg)
      : SaboCoverUICaboDriverBase(cover_ui_cfg), pins_(cover_ui_cfg_->pins.v01) {
  }

  bool Init() override {
    if (!SaboCoverUICaboDriverBase::Init()) return false;

    // Init Cabo's control pins
    palSetLineMode(pins_.latch_load, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2);
    palWriteLine(pins_.latch_load, PAL_LOW);  // HC595 RCLK/PL (parallel load)

    palSetLineMode(pins_.oe, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2);
    palWriteLine(pins_.oe, PAL_LOW);  // HW v0.1 has an HEF4794BT OE driver which inverts the signal

    palSetLineMode(pins_.btn_cs, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2);
    palWriteLine(pins_.btn_cs, PAL_HIGH);  // /CS (chip select = no)

    return true;
  }

  // Latch LEDs as well as button-row, and load button columns
  // Due to combined HC595 RCLK and HC165 SH/PL, we need to do this in Full Duplex mode for lesser LED glowing
  void LatchLoad() override {
    if (!series_) return;

    uint8_t tx_data = current_led_mask_ | series_->GetButtonRowMask();
    uint8_t rx_data;

    // SPI transfer LEDs+ButtonRow and read button for previously set row
    spiAcquireBus(cover_ui_cfg_->spi.instance);

    spiStart(cover_ui_cfg_->spi.instance, &spi_config_);
    // Enable HC165 shifting, but this will also set HEF4794BT latch open! = low-glowing LEDs
    palWriteLine(pins_.latch_load, PAL_HIGH);
    palWriteLine(pins_.btn_cs, PAL_LOW);
    spiExchange(cover_ui_cfg_->spi.instance, 1, &tx_data, &rx_data);  // Full duplex send and receive
    palWriteLine(pins_.btn_cs, PAL_HIGH);
    palWriteLine(pins_.latch_load, PAL_LOW);  // Close HEF4794BT latch (and /PL of HC165)

    spiReleaseBus(cover_ui_cfg_->spi.instance);

    // Buffer / Shift & buffer depending on current row, as well as advance to next row
    // FIXME: Use now DebounceRawButtons()
    // btn_cur_raw_mask_ = series_->ProcessButtonCol(btn_cur_raw_mask_, rx_data);
    DebounceRawButtons(series_->ProcessButtonCol(rx_data));  // Process received button column
  };

 protected:
  SaboCoverUISeriesInterface* GetSeriesDriver() override {
    // HW v0.1 does only support CoverUI Series-II and does NOT support detection
    static SaboCoverUISeries2 series2_driver;
    return &series2_driver;
  }

  uint8_t MapLedIdToMask(LedId id) const override {
    return series_ ? series_->MapLedIdToMask(id) : 0;
  };

 private:
  const decltype(xbot::driver::sabo::config::CoverUi::pins.v01)& pins_;
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_CABO_DRIVER_V01_HPP
