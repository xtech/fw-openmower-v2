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
  explicit SaboCoverUICaboDriverV01(CaboCfg cabo_cfg) : SaboCoverUICaboDriverBase(cabo_cfg) {
  }

  bool Init() override {
    if (!SaboCoverUICaboDriverBase::Init()) return false;

    // HW v0.1 has an HEF4794BT OE driver which inverts the signal. Newer boards will not have this driver anymore.
    palWriteLine(cabo_cfg_.pins.oe, PAL_LOW);

    spi_config_ = {
        .circular = false,
        .slave = false,
        .data_cb = NULL,
        .error_cb = NULL,
        .ssline = 0,
        .cfg1 = SPI_CFG1_MBR_0 | SPI_CFG1_MBR_1 |  // Baudrate = FPCLK/16 (12.5 MHz @ 200 MHz PLL2_P)
                SPI_CFG1_DSIZE_2 | SPI_CFG1_DSIZE_1 | SPI_CFG1_DSIZE_0,  // 8-Bit (DS = 0b111)*/
        .cfg2 = SPI_CFG2_MASTER  // Master, Mode 0 (CPOL=0, CPHA=0) = Data on rising edge
    };

    return true;
  }

  // Latch LEDs as well as button-row, and load button columns
  // Due to combined HC595 RCLK and HC165 SH/PL, we need to do this in Full Duplex mode for lesser LED glowing
  void LatchLoad() override {
    if (!series_) return;

    uint8_t tx_data = current_led_mask_ | series_->GetButtonRowMask();
    uint8_t rx_data;

    // SPI transfer LEDs+ButtonRow and read button for previously set row
    spiAcquireBus(cabo_cfg_.spi.instance);

    // Enable HC165 shifting, but this will also set HEF4794BT latch open! = low-glowing LEDs
    palWriteLine(cabo_cfg_.pins.latch_load, PAL_HIGH);
    palWriteLine(cabo_cfg_.pins.btn_cs, PAL_LOW);
    spiExchange(cabo_cfg_.spi.instance, 1, &tx_data, &rx_data);  // Full duplex send and receive
    palWriteLine(cabo_cfg_.pins.btn_cs, PAL_HIGH);
    palWriteLine(cabo_cfg_.pins.latch_load, PAL_LOW);  // Close HEF4794BT latch (and /PL of HC165)

    spiReleaseBus(cabo_cfg_.spi.instance);

    // Buffer / Shift & buffer depending on current row, as well as advance to next row
    // FIXME: Use now DebounceRawButtons()
    // btn_cur_raw_mask_ = series_->ProcessButtonCol(btn_cur_raw_mask_, rx_data);
    DebounceRawButtons(series_->ProcessButtonCol(rx_data));  // Process received button column
  };

 protected:
  SaboCoverUISeriesInterface* GetSeriesDriver() override {
    // HW v0.1 does only support CoverUI Series-II and does NOT support detection
    static SaboCoverUISeries2V01 series2_driver;
    return &series2_driver;
  }

  uint8_t MapLEDIDToMask(LEDID id) const override {
    return series_ ? series_->MapLEDIDToMask(id) : 0;
  };
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_CABO_DRIVER_V01_HPP
