/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sabo_cover_ui_cabo_driver_v03.hpp
 * @brief Sabo CoverUI Cabo Driver for Hardware v0.3 with TCA9534/TCA9535 GPIO expanders
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-03-02
 */

#ifndef OPENMOWER_SABO_COVER_UI_CABO_DRIVER_V03_HPP
#define OPENMOWER_SABO_COVER_UI_CABO_DRIVER_V03_HPP

#include <ulog.h>

#include <cassert>

#include "drivers/gpio/tca_95xx/tca9534.hpp"
#include "drivers/gpio/tca_95xx/tca9535.hpp"
#include "sabo_cover_ui_cabo_driver_base.hpp"
#include "sabo_cover_ui_series1_v03.hpp"
#include "sabo_cover_ui_series2.hpp"

namespace xbot::driver::ui {

using namespace xbot::driver::sabo::types;
using namespace xbot::driver::gpio;

// Sabo CoverUI Driver for Hardware v0.3
class SaboCoverUICaboDriverV03 : public SaboCoverUICaboDriverBase {
 public:
  explicit SaboCoverUICaboDriverV03(const xbot::driver::sabo::config::CoverUi* cover_ui_cfg)
      : SaboCoverUICaboDriverBase(cover_ui_cfg),
        gpio_exp_leds_(&cover_ui_cfg->gpio_expander.leds),
        gpio_exp_btns_(&cover_ui_cfg->gpio_expander.btns),
        pins_(cover_ui_cfg_->pins.v03) {
  }

  // Relevant GPIO bits for CoverUI-Series detection (/CON)
  static constexpr uint8_t GPIO_LEDS_MASK_S1_CONNECTED_L = (1 << 5);   // Series-I CoverUI /Connected
  static constexpr uint16_t GPIO_BTNS_MASK_S2_CONNECTED_L = (1 << 9);  // Series-II CoverUI /Connected

  static constexpr uint16_t GPIO_BTNS_PIN_S2_OE = 10;  // Series-II OE

  bool Init() override {
    if (!SaboCoverUICaboDriverBase::Init()) return false;

    // Non-sense SPI-MOSI switch. Series-I doesn't need it why it needs to be permanent on LCD selection = high
    // Series-II has remorked bridged outputs, wherefor it doesn't matter
    palSetLineMode(pins_.sel_spi_mosi, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_PULLUP);
    palWriteLine(pins_.sel_spi_mosi, PAL_HIGH);

    // None-sense SPI-MISO switch. BMC got identified in-between as I2C instead of designed SPI.
    // Permanently set it to UI-S2-MISO = low
    palSetLineMode(pins_.sel_spi_miso, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_PULLDOWN);
    palWriteLine(pins_.sel_spi_miso, PAL_LOW);

    // Set HEF4794BT STR (LEDs and button row) to low by default before Series detection / initialization
    palSetLineMode(pins_.s2_latch, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_PULLUP);
    palWriteLine(pins_.s2_latch, PAL_LOW);

    palSetLineMode(pins_.s2_load, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_PULLUP);
    palWriteLine(pins_.s2_load, PAL_LOW);  // Set S2-Load to low to block HC165 shifting

    // Initialize GPIO expanders
    if (!gpio_exp_leds_.Init()) {
      return false;
    }
    if (!gpio_exp_btns_.Init()) {
      return false;
    }

    // Configure GPIO expander for Series-I LEDs
    if (!gpio_exp_leds_.ConfigPort(~SaboCoverUISeries1V03::GPIO_LED_PINS)) {  // 0 = output
      return false;
    }

    // Configure GPIO expander for Series-I Buttons, S2-/CON and S2-OE
    if (!gpio_exp_btns_.ConfigPort(~(1 << GPIO_BTNS_PIN_S2_OE))) {  // S2-OE is output, the rest are inputs
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
        // LEDs are 5V driven and are low active, but our GPIO Expander is 3V3, thus a high level still let the red LED
        // glow (1.7V voltage diff). To avoid that, switch disabled LEDs to input mode.
        gpio_exp_leds_.ConfigPort(SaboCoverUISeries1V03::GPIO_LED_PINS & ~current_led_mask_);  // 0 = output, 1 = input
        gpio_exp_leds_.WritePort(~current_led_mask_);  // Series-I LEDs are active low, so invert the mask

        // Buttons
        uint16_t btns;
        gpio_exp_btns_.ReadPort(&btns);
        btns &= SaboCoverUISeries1V03::GPIO_BTNS_MASK_BTN_ALL_L;   // Mask out buttons
        btns |= ~SaboCoverUISeries1V03::GPIO_BTNS_MASK_BTN_ALL_L;  // Set non-button bits to 1 for easier diagnostics
        DebounceRawButtons(btns);
        break;
      case SeriesType::Series2:
        // Series2LatchSSR(current_led_mask_ | series_->GetButtonRowMask());  // Latch LEDs and button row
        btn_cols = Series2LatchLoad(current_led_mask_ |
                                    series_->GetButtonRowMask());  // Latch LEDs and button row, return button columns
        DebounceRawButtons(series_->ProcessButtonCol(btn_cols));   // Process received button column
        break;
      default: ULOG_ERROR("Unknown Sabo CoverUI series type"); return;
    }
  };

 protected:
  TCA9534Driver gpio_exp_leds_;  // GPIO expander for Series-I LEDs (and Series-I connection detection))
  TCA9535Driver gpio_exp_btns_;  // GPIO expander for Series-I buttons as well as Series-II /con and OE

  uint8_t MapLedIdToMask(const LedId id) const override {
    //  LedId ENUM matches LEDs pin position for both series now
    return (1 << uint8_t(id)) & 0b11111;  // Safety mask to only use the connected LEDs
  }

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

    uint16_t gpio_port_data = 0;

    // Series-I CoverUI has it's /Connected signal on the LEDs- GPIO expander
    if (!gpio_exp_leds_.ReadPort(&gpio_port_data)) {
      ULOG_ERROR("Failed to read from GPIO LEDs expander, cannot detect Sabo CoverUI");
      return nullptr;
    }
    // ULOG_INFO("GPIO LEDs expander read value: 0x%04X", gpio_port_data);

    if ((gpio_port_data & GPIO_LEDS_MASK_S1_CONNECTED_L) == 0) {
      ULOG_INFO("Detected Sabo Series-I CoverUI");
      static SaboCoverUISeries1V03 series1_driver;
      return &series1_driver;
    }

    // Series-II CoverUI has it's /Connected signal on the BTNs- GPIO expander
    if (!gpio_exp_btns_.ReadPort(&gpio_port_data)) {
      ULOG_ERROR("Failed to read from GPIO Buttons expander, cannot detect Sabo CoverUI");
      return nullptr;
    }
    ULOG_INFO("GPIO BTNs expander read value: 0x%04X", gpio_port_data);

    if ((gpio_port_data & GPIO_BTNS_MASK_S2_CONNECTED_L) == 0) {
      ULOG_INFO("Detected Sabo Series-II CoverUI");

      Series2LatchLoad(0);  // Latch LEDs=off and select no button row

      // Set UI_S2-OE to enable Series-II HEF4794 outputs (LEDs and button row)
      gpio_exp_btns_.WritePin(GPIO_BTNS_PIN_S2_OE, true);

      static SaboCoverUISeries2 series2_driver;
      return &series2_driver;
    }

    ULOG_INFO("No Sabo CoverUI detected");
    return nullptr;
  }

 private:
  const decltype(xbot::driver::sabo::config::CoverUi::pins.v03)& pins_;
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_CABO_DRIVER_V03_HPP
