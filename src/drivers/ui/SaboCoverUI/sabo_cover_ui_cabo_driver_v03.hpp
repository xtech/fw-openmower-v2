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
 * @date 2025-02-20
 */

#ifndef OPENMOWER_SABO_COVER_UI_CABO_DRIVER_V03_HPP
#define OPENMOWER_SABO_COVER_UI_CABO_DRIVER_V03_HPP

#include <ulog.h>

#include <cassert>

#include "drivers/gpio/tca_95xx/tca9534.hpp"
#include "drivers/gpio/tca_95xx/tca9535.hpp"
#include "sabo_cover_ui_cabo_driver_base.hpp"
#include "sabo_cover_ui_series1_v03.hpp"
// #include "sabo_cover_ui_series2.hpp"

namespace xbot::driver::ui {

using namespace xbot::driver::sabo::types;
using namespace xbot::driver::gpio;

// Sabo CoverUI Driver for Hardware v0.3
class SaboCoverUICaboDriverV03 : public SaboCoverUICaboDriverBase {
 public:
  explicit SaboCoverUICaboDriverV03(const xbot::driver::sabo::config::CoverUi* cover_ui_cfg)
      : SaboCoverUICaboDriverBase(cover_ui_cfg),
        gpio_exp_leds_(&cover_ui_cfg->gpio_expander.leds),
        gpio_exp_btns_(&cover_ui_cfg->gpio_expander.btns) {
  }

  // Relevant control bits of 74HC595 shift register
  /*static constexpr uint8_t SR_MASK_S2_HALL_EN_L    = (1 << 0);  // Series-II Hall /EN
  static constexpr uint8_t SR_MASK_S2_LATCH_LOAD_L = (1 << 1);  // Series-II Latch- HEF4794BT, /Load 74HC165 BUT MODded
  to LINE_UART7_RX */

  // Relevant GPIO bits for CoverUI-Series detection (/CON)
  static constexpr uint8_t GPIO_LEDS_MASK_S1_CONNECTED_L = (1 << 5);   // Series-I CoverUI /Connected
  static constexpr uint16_t GPIO_BTNS_MASK_S2_CONNECTED_L = (1 << 9);  // Series-II CoverUI /Connected

  static constexpr uint16_t GPIO_BTNS_PIN_S2_OE = 10;  // Series-II OE

  bool Init() override {
    if (!SaboCoverUICaboDriverBase::Init()) return false;

    spi_config_ = {
        .circular = false,
        .slave = false,
        .data_cb = NULL,
        .error_cb = NULL,
        .ssline = 0,
        // HEF4794BT is the slowest device on SPI bus. F_clk(max)@5V: Min=5MHz, Typ=10MHz
        // Also worked with 12.5MHz, but let's be save within the limits of the HEF4794BT
        .cfg1 = SPI_CFG1_MBR_2 | SPI_CFG1_MBR_0 |  // Baudrate = FPCLK/32 (6.25 MHz @ 200 MHz PLL2_P)
                SPI_CFG1_DSIZE_2 | SPI_CFG1_DSIZE_1 | SPI_CFG1_DSIZE_0,  // 8-Bit (DS = 0b111)*/
        .cfg2 = SPI_CFG2_MASTER  // Master, Mode 0 (CPOL=0, CPHA=0) = Data on rising edge
    };

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
        Series2LatchSR(current_led_mask_ | series_->GetButtonRowMask());  // Latch LEDs and button row
        LatchLoadSR();                                                    // Load raw buttons
        DebounceRawButtons(series_->ProcessButtonCol(sr_load_buf_[2]));   // Process received button column
        break;
      default: ULOG_ERROR("Unknown Sabo CoverUI series type"); return;
    }
  };

 protected:
  TCA9534Driver gpio_exp_leds_;  // GPIO expander for Series-I LEDs (and Series-I connection detection))
  TCA9535Driver gpio_exp_btns_;  // GPIO expander for Series-I buttons as well as Series-II /con and OE

  uint8_t cabo_sr_ctrl_mask_;  // 74HC595 Control bitmask of CarrierBoard

  // HC165 parallel load shift register
  uint8_t sr_load_buf_[3];
  size_t sr_load_size_ = 2;  // Get set to 3 if a Series-II CoverUI get detected

  // Cabo's HC165 input mask
  uint16_t sr_inp_mask_ = 0xFFFF;  // All inputs are high (inactive)

  uint8_t MapLedIdToMask(const LedId id) const override {
    //  LedId ENUM matches LEDs pin position for both series now
    return (1 << uint8_t(id)) & 0b11111;  // Safety mask to only use the connected LEDs
  }

  /**
   * @brief Send tx_data to Series-II HEF4794 and latch it.
   *
   * @param tx_data
   */
  void Series2LatchSR(uint8_t tx_data) {
    spiAcquireBus(cover_ui_cfg_->spi.instance);

    spiStart(cover_ui_cfg_->spi.instance, &spi_config_);
    spiSend(cover_ui_cfg_->spi.instance, 1, &tx_data);  // Send tx_data to HEF4794
    palWriteLine(UI_S2_LATCH, PAL_HIGH);                // Latch HEF4794
    chThdSleepMicroseconds(1);
    palWriteLine(UI_S2_LATCH, PAL_LOW);  // Close HEF4794 latch

    spiReleaseBus(cover_ui_cfg_->spi.instance);
  }

  /**
   * @brief Latch Cabo's HC595 and load HC165 parallel-input shift register.
   * It's required to do this in two steps, because HC595 get latched by rising edge of SH/PL of HC165
   */
  void LatchLoadSR() {
    assert(sr_load_size_ <= sizeof(sr_load_buf_));

    spiAcquireBus(cover_ui_cfg_->spi.instance);

    spiStart(cover_ui_cfg_->spi.instance, &spi_config_);
    palWriteLine(cover_ui_cfg_->pins.latch_load, PAL_LOW);      // HC165 /PL (parallel load) pulse, also blocks shifting
    if (sr_load_size_ == 3) palWriteLine(UI_S2_LOAD, PAL_LOW);  // S2- /PL
    chThdSleepMicroseconds(1);
    spiSend(cover_ui_cfg_->spi.instance, 1, &cabo_sr_ctrl_mask_);  // Send data to HC595
    chThdSleepMicroseconds(1);
    palWriteLine(cover_ui_cfg_->pins.latch_load, PAL_HIGH);      // HC165 shift enable & latch HC595 (RCLK rising edge)
    if (sr_load_size_ == 3) palWriteLine(UI_S2_LOAD, PAL_HIGH);  // S2- HC165 shift enable
    chThdSleepMicroseconds(1);
    spiReceive(cover_ui_cfg_->spi.instance, sr_load_size_, sr_load_buf_);
    palWriteLine(cover_ui_cfg_->pins.latch_load, PAL_LOW);      // Need to block HC165 shifting again
    if (sr_load_size_ == 3) palWriteLine(UI_S2_LOAD, PAL_LOW);  // S2- /PL

    spiReleaseBus(cover_ui_cfg_->spi.instance);

    // Extract Cabo's sr_inp_mask_ out of the received bytes, which may differ dependent of the connected CoverUI
    // Series. If a Series-II is connected it shift its own HC165 byte first.
    sr_inp_mask_ = (sr_load_buf_[sr_load_size_ - 1] << 8) | sr_load_buf_[sr_load_size_ - 2];
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
      gpio_exp_btns_.WritePin(GPIO_BTNS_PIN_S2_OE, true);  // Set S2-OE to enable Series-II HC165 outputs
      // TODO
      // Series2LatchSR(0);                            // Latch LEDs=off and select no button row
      // LatchLoadSR();
      // sr_load_size_ = 3;  // For Series-II CoverUI we need to read 3 bytes for the HC165
      static SaboCoverUISeries2 series2_driver;
      return &series2_driver;
    }

    ULOG_INFO("No Sabo CoverUI detected");
    return nullptr;
  }
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_CABO_DRIVER_V03_HPP
