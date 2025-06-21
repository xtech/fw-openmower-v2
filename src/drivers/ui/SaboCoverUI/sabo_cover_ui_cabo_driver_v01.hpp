//
// Created by Apehaenger on 6/1/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_CABO_DRIVER_V01_HPP
#define OPENMOWER_SABO_COVER_UI_CABO_DRIVER_V01_HPP

#include "sabo_cover_ui_cabo_driver_base.hpp"
#include "sabo_cover_ui_series2_v01.hpp"

namespace xbot::driver::ui {

// Sabo CoverUI Driver for Hardware v0.1
class SaboCoverUICaboDriverV01 : public SaboCoverUICaboDriverBase {
 public:
  explicit SaboCoverUICaboDriverV01(const DriverConfig& config) : SaboCoverUICaboDriverBase(config) {
  }

  bool Init() override {
    if (!SaboCoverUICaboDriverBase::Init()) return false;

    // HW v0.1 does only support CoverUI Series-II
    static SaboCoverUISeries2V01 series2_driver;
    series_ = &series2_driver;

    // HW v0.1 has an HEF4794BT OE driver which inverts the signal. Newer boards will not have this driver anymore.
    palWriteLine(config_.control_pins.oe, PAL_LOW);

    return true;
  }

  // Latch LEDs as well as button-row, and load button columns
  // Due to combined HC595 RCLK and HC165 SH/PL, we need to do this in Full Duplex mode for lesser LED glowing
  void LatchLoad() override {
    uint8_t tx_data = current_led_mask_ | series_->GetButtonRowMask();
    uint8_t rx_data;

    // SPI transfer LEDs+ButtonRow and read button for previously set row
    spiAcquireBus(config_.spi_instance);
    // Enable HC165 shifting, but this will also set HEF4794BT latch open! = low-glowing LEDs
    palWriteLine(config_.control_pins.latch_load, PAL_HIGH);
    palWriteLine(config_.control_pins.btn_cs, PAL_LOW);
    spiExchange(config_.spi_instance, 1, &tx_data, &rx_data);  // Full duplex send and receive
    palWriteLine(config_.control_pins.btn_cs, PAL_HIGH);
    palWriteLine(config_.control_pins.latch_load, PAL_LOW);  // Close HEF4794BT latch (and /PL of HC165)
    spiReleaseBus(config_.spi_instance);

    // Buffer / Shift & buffer depending on current row, as well as advance to next row
    // FIXME: Use now DebounceRawButtons()
    // btn_cur_raw_mask_ = series_->ProcessButtonCol(btn_cur_raw_mask_, rx_data);
    DebounceRawButtons(series_->ProcessButtonCol(rx_data));  // Process received button column
  };

 protected:
  uint8_t MapLEDIDToMask(LEDID id) const override {
    return series_ ? series_->MapLEDIDToMask(id) : 0;
  };
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_CABO_DRIVER_V01_HPP
