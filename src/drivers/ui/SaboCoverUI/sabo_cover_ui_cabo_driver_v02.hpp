//
// Created by Apehaenger on 6/5/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_CABO_DRIVER_V02_HPP
#define OPENMOWER_SABO_COVER_UI_CABO_DRIVER_V02_HPP

#include <ulog.h>

#include <cassert>

#include "sabo_cover_ui_cabo_driver_base.hpp"
#include "sabo_cover_ui_series1_v02.hpp"
#include "sabo_cover_ui_series2.hpp"

// HW v0.2 messed up control pins
#define UI_S2_LATCH LINE_GPIO8    // Series-II Latch (HEF4794BT)
#define UI_S2_LOAD LINE_UART7_RX  // Series-II SH/LD (HC165)

namespace xbot::driver::ui {

// Sabo CoverUI Driver for Hardware v0.2
class SaboCoverUICaboDriverV02 : public SaboCoverUICaboDriverBase {
 public:
  explicit SaboCoverUICaboDriverV02(const DriverConfig& config) : SaboCoverUICaboDriverBase(config) {
  }

  // clang-format off
  // Relevant control bits of 74HC595 shift register
  static constexpr uint8_t SR_MASK_S2_HALL_EN_L    = (1 << 0);  // Series-II Hall /EN
  static constexpr uint8_t SR_MASK_S2_LATCH_LOAD_L = (1 << 1);  // Series-II Latch- HEF4794BT, /Load 74HC165 BUT MODded to LINE_UART7_RX
  static constexpr uint8_t SR_MASK_S2_OE           = (1 << 2);  // Series-II HEF4794BT OE

  // Relevant input bits of 74HC165 parallel-input shift register
  static constexpr uint16_t INP_MASK_S1_CONNECTED_L  = (1 << 2);  // Series-I CoverUI /Connected
  static constexpr uint16_t INP_MASK_S2_CONNECTED_L  = (1 << 9);  // Series-II CoverUI /Connected
  static constexpr uint16_t INP_MASK_JP3_L           = (1 << 11); // Mainboard solder jumper JP3

  // See also LED and Button bits in sabo_cover_ui_series1_v02.hpp

  // clang-format on

  bool Init() override {
    if (!SaboCoverUICaboDriverBase::Init()) return false;

    // Init HW v0.2 messed up control pins
    palSetLineMode(UI_S2_LATCH, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_PULLDOWN);
    palWriteLine(UI_S2_LATCH, PAL_LOW);  // Close HEF4794 latch

    palSetLineMode(UI_S2_LOAD, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_PULLUP);
    palWriteLine(UI_S2_LOAD, PAL_LOW);  // /PL (parallel load) of HC165 = NO shifting

    // Cabo HC165 /CS is superfluous because of combined HC595 RCLK and HC165 SH/PL
    // and the resulting Send+Receive cycle requirement
    palWriteLine(config_.control_pins.inp_cs, PAL_LOW);

    return true;
  }

  // Latch LEDs (as well as button-row if existent) and load button columns
  void LatchLoad() override {
    if (!series_) {
      series_ = GetSeriesDriver();
    }
    if (!series_) return;

    switch (series_->GetType()) {
      case SeriesType::Series1:
        cabo_sr_ctrl_mask_ &= ~SaboCoverUISeries1V02::SR_MASK_LED_ALL_L;  // Clear all Series-I LED bits
        cabo_sr_ctrl_mask_ |= (~current_led_mask_ & SaboCoverUISeries1V02::SR_MASK_LED_ALL_L);  // Invert current LEDs
        LatchLoadSR();  // Latch LEDs and load raw buttons
        DebounceRawButtons((sr_inp_mask_ & SaboCoverUISeries1V02::INP_MASK_BTN_ALL_L) |  // Debounce only button bits
                           (~SaboCoverUISeries1V02::INP_MASK_BTN_ALL_L));  // The others set to 1 for easier diagnostics
        break;
      case SeriesType::Series2:
        Series2LatchSR(current_led_mask_ | series_->GetButtonRowMask());  // Latch LEDs and button row
        LatchLoadSR();                                                    // Load raw buttons
        DebounceRawButtons(series_->ProcessButtonCol(sr_load_buf_[2]));   // Process received button column
        break;
      default: ULOG_ERROR("Unknown CoverUI series type"); return;
    }
  };

 protected:
  uint8_t cabo_sr_ctrl_mask_;  // 74HC595 Control bitmask of CarrierBoard

  // HC165 parallel load shift register
  uint8_t sr_load_buf_[3];
  size_t sr_load_size_ = 2;  // Get set to 3 if a Series-II CoverUI get detected

  // Cabo's HC165 input mask
  uint16_t sr_inp_mask_ = 0xFFFF;  // All inputs are high (inactive)

  uint8_t MapLEDIDToMask(LEDID id) const override {
    return series_ ? series_->MapLEDIDToMask(id) : 0;
  };

  /**
   * @brief Send tx_data to Series-II HEF4794 and latch it.
   *
   * @param tx_data
   */
  void Series2LatchSR(uint8_t tx_data) {
    spiAcquireBus(config_.spi_instance);
    spiSend(config_.spi_instance, 1, &tx_data);  // Send tx_data to HEF4794
    palWriteLine(UI_S2_LATCH, PAL_HIGH);         // Latch HEF4794
    chThdSleepMicroseconds(1);
    palWriteLine(UI_S2_LATCH, PAL_LOW);  // Close HEF4794 latch
    spiReleaseBus(config_.spi_instance);
  }

  /**
   * @brief Latch Cabo's HC595 and load HC165 parallel-input shift register.
   * It's required to do this in two steps, because HC595 get latched by rising edge of SH/PL of HC165
   */
  void LatchLoadSR() {
    assert(sr_load_size_ <= sizeof(sr_load_buf_));

    spiAcquireBus(config_.spi_instance);

    palWriteLine(config_.control_pins.latch_load, PAL_LOW);     // HC165 /PL (parallel load) pulse, also blocks shifting
    if (sr_load_size_ == 3) palWriteLine(UI_S2_LOAD, PAL_LOW);  // S2- /PL
    chThdSleepMicroseconds(1);
    spiSend(config_.spi_instance, 1, &cabo_sr_ctrl_mask_);  // Send data to HC595
    chThdSleepMicroseconds(1);
    palWriteLine(config_.control_pins.latch_load, PAL_HIGH);     // HC165 shift enable & latch HC595 (RCLK rising edge)
    if (sr_load_size_ == 3) palWriteLine(UI_S2_LOAD, PAL_HIGH);  // S2- HC165 shift enable
    chThdSleepMicroseconds(1);
    spiReceive(config_.spi_instance, sr_load_size_, sr_load_buf_);
    palWriteLine(config_.control_pins.latch_load, PAL_LOW);     // Need to block HC165 shifting again
    if (sr_load_size_ == 3) palWriteLine(UI_S2_LOAD, PAL_LOW);  // S2- /PL

    spiReleaseBus(config_.spi_instance);

    // Extract Cabo's sr_inp_mask_ out of the received bytes, which may differ dependent of the connected CoverUI
    // Series. If a Series-II is connected it shift its own HC165 byte first.
    sr_inp_mask_ = (sr_load_buf_[sr_load_size_ - 1] << 8) | sr_load_buf_[sr_load_size_ - 2];
  }

  SaboCoverUISeriesInterface* GetSeriesDriver() {
    static systime_t next_process_time = 0;

    if (chVTGetSystemTimeX() < next_process_time) {
      return nullptr;
    }
    next_process_time = chVTGetSystemTimeX() + TIME_S2I(2);  // Process only every 2nd second

    // Set default signals: S2_Hall = off, S2-OE = off, All S1-LEDs off
    cabo_sr_ctrl_mask_ = SR_MASK_S2_HALL_EN_L | SaboCoverUISeries1V02::SR_MASK_LED_ALL_L;
    LatchLoadSR();  // Latch ctrl mask and read the parallel-input shift register to get the two /CON signals

    if ((sr_inp_mask_ & INP_MASK_S1_CONNECTED_L) == 0) {
      ULOG_INFO("Detected Series-I CoverUI");
      static SaboCoverUISeries1V02 series1_driver;
      return &series1_driver;
    }

    if ((sr_inp_mask_ & INP_MASK_S2_CONNECTED_L) == 0) {
      ULOG_INFO("Detected Series-II CoverUI");
      Series2LatchSR(0);                            // Latch LEDs=off and select no button row
      cabo_sr_ctrl_mask_ &= ~SR_MASK_S2_HALL_EN_L;  // S2-Hall = on
      cabo_sr_ctrl_mask_ |= SR_MASK_S2_OE;          // S2-OE = on
      LatchLoadSR();
      sr_load_size_ = 3;  // For Series-II CoverUI we need to read 3 bytes for the HC165
      static SaboCoverUISeries2 series2_driver;
      return &series2_driver;
    }

    ULOG_INFO("No Sabo CoverUI detected [sr_inp_mask_ = 0x%04X]", sr_inp_mask_);
    return nullptr;
  }
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_CABO_DRIVER_V02_HPP
