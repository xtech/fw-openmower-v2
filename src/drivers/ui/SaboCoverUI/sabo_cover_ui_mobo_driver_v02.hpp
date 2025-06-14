//
// Created by Apehaenger on 6/5/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_MOBO_DRIVER_V02_HPP
#define OPENMOWER_SABO_COVER_UI_MOBO_DRIVER_V02_HPP

#include <ulog.h>

#include <cassert>

#include "sabo_cover_ui_mobo_driver_base.hpp"
#include "sabo_cover_ui_series1.hpp"
#include "sabo_cover_ui_series2.hpp"

namespace xbot::driver::ui {

// Sabo CoverUI Driver for Hardware v0.2
class SaboCoverUIMoboDriverV02 : public SaboCoverUIMoboDriverBase {
 public:
  explicit SaboCoverUIMoboDriverV02(const DriverConfig& config) : SaboCoverUIMoboDriverBase(config) {
  }

  // clang-format off
  // Control bits of 74HC595 shift register
  static constexpr uint8_t SR_MASK_S2_HALL_EN_L     = (1 << 0);  // Series-II Hall /EN
  static constexpr uint8_t SR_MASK_S2_LATCH_LOAD_L  = (1 << 1);  // Series-II Latch- HEF4794BT, /Load 74HC165
  static constexpr uint8_t SR_MASK_S2_OE            = (1 << 2);  // Series-II HEF4794BT OE
  // Series-I LED bits of 74HC595 shift register
  static constexpr uint8_t SR_MASK_S1_LED_AUTO_L    = (1 << 3);
  static constexpr uint8_t SR_MASK_S1_LED_HOME_L    = (1 << 4);
  static constexpr uint8_t SR_MASK_S1_LED_PLAY_RD_L = (1 << 5);
  static constexpr uint8_t SR_MASK_S1_LED_MOW_L     = (1 << 6);
  static constexpr uint8_t SR_MASK_S1_LED_PLAY_GN_L = (1 << 7);
  static constexpr uint8_t SR_MASK_S1_LED_ALL_L     = (SR_MASK_S1_LED_AUTO_L | SR_MASK_S1_LED_HOME_L |
    SR_MASK_S1_LED_PLAY_RD_L | SR_MASK_S1_LED_MOW_L | SR_MASK_S1_LED_PLAY_GN_L);

  // Input bits of 74HC165 parallel-input shift register
  static constexpr uint16_t INP_MASK_S1_BTN_SELECT_L = (1 << 0);  // Series-I Button "Select"
  static constexpr uint16_t INP_MASK_S1_BTN_MENUE_L  = (1 << 1);
  static constexpr uint16_t INP_MASK_S1_CONNECTED_L  = (1 << 2);  // Series-I CoverUI /Connected
  static constexpr uint16_t INP_MASK_S1_BTN_PLAY_L   = (1 << 3);
  static constexpr uint16_t INP_MASK_S1_BTN_RIGHT_L  = (1 << 4);
  static constexpr uint16_t INP_MASK_S1_BTN_BACK_L   = (1 << 5);
  static constexpr uint16_t INP_MASK_S1_BTN_LEFT_L   = (1 << 6);
  static constexpr uint16_t INP_MASK_S1_BTN_UP_L     = (1 << 7);
  static constexpr uint16_t INP_MASK_S1_BTN_DOWN_L   = (1 << 8);
  static constexpr uint16_t INP_MASK_S2_CONNECTED_L  = (1 << 9);  // Series-II CoverUI /Connected
  static constexpr uint16_t INP_MASK_S1_BTN_OK_L     = (1 << 10); // Modified JP2
  static constexpr uint16_t INP_MASK_JP3_L           = (1 << 11); // Mainboard solder jumper JP3
  // clang-format on

  // Latch LEDs as well as button-row, and load button columns
  void LatchLoad() override {
    if (!series_) {
      series_ = GetSeriesDriver();
    }
    if (!series_) return;

    uint8_t tx_data = 0;
    switch (series_->GetType()) {
      case SeriesType::Series1:
        // Series-I: Latch data for ctrl-mask and LEDs
        tx_data = sr_ctrl_mask_ | (~current_led_mask_ & SR_MASK_S1_LED_ALL_L);
        MoboLatchSR(tx_data);
        // TODO: Read Buttons
        break;
      case SeriesType::Series2:
        // Series-II: Latch LEDs and button row
        tx_data = sr_ctrl_mask_ | series_->GetButtonRowMask();
        break;
      default: ULOG_ERROR("Unknown CoverUI series type"); return;
    }

    // Prepare tx data and merge Ctrl-bits with LED-bits
    /*uint8_t tx_data = sr_ctrlBits.to_ulong() | srLedBits.to_ulong();
    uint8_t rx_data = LatchLoadRaw(tx_data);  // Latch LEDs and load button row

    // Buffer / Shift & buffer depending on current row, as well as advance to next row
    btn_cur_raw_mask_ = series_->ProcessButtonCol(btn_cur_raw_mask_, rx_data);*/
  };

  // Latch tx_data and return loaded data (button columns)
  /*uint8_t LatchLoadRaw(uint8_t tx_data) {
    uint8_t rx_data = 0;

    // Load 74HC165 shift register (the very first load will load nothing, because row is not set yet. Who cares)
    palWriteLine(config_.control_pins.latch_load, PAL_LOW);  // /PL (parallel load) Pulse
    chThdSleepMicroseconds(1);
    palWriteLine(config_.control_pins.latch_load, PAL_HIGH);

    // SPI transfer LEDs+ButtonRow and read button for previously set row
    spiAcquireBus(config_.spi_instance);
    palWriteLine(config_.control_pins.latch_load, PAL_HIGH);   // Latch HEF4794BT (STR = HIGH)
    palWriteLine(config_.control_pins.btn_cs, PAL_LOW);        // HC165 /CE
    spiExchange(config_.spi_instance, 1, &tx_data, &rx_data);  // Full duplex send and receive
    palWriteLine(config_.control_pins.latch_load, PAL_LOW);    // Stop latching (STR = LOW)
    palWriteLine(config_.control_pins.btn_cs, PAL_HIGH);
    spiReleaseBus(config_.spi_instance);

    return rx_data;
  };*/

  void Tick() override {
    SaboCoverUIMoboDriverBase::Tick();
  };

 protected:
  // 74HC595 Control bitmask
  uint8_t sr_ctrl_mask_ = SR_MASK_S2_HALL_EN_L;  // S2_Hall = off, S2-Load (blocked), S2-OE = off

  // HC165 parallel load shift register
  uint8_t sr_load_buf_[3];
  unsigned int sr_load_size_ = 2;  // Get only set to 3 if a Series-II CoverUI get detected

  // Mobo HC165 input mask
  uint16_t sr_inp_mask_ = 0xFFFF;  // All inputs are high (inactive)

  uint8_t MapLEDIDToBit(LEDID id) const override {
    return series_ ? series_->MapLEDIDToBit(id) : 0;
  };

  /**
   * @brief Send tx_data to the mainboards HC595 shift register and latch it.
   *
   * @param tx_data
   */
  void MoboLatchSR(uint8_t tx_data) {
    spiAcquireBus(config_.spi_instance);
    palWriteLine(config_.control_pins.latch_load, PAL_LOW);   // Close HC595 latch (RCLK
    spiSend(config_.spi_instance, 1, &tx_data);               // Send tx_data to HC595
    palWriteLine(config_.control_pins.latch_load, PAL_HIGH);  // HC595 Latch (RCLK rising edge)
    spiReleaseBus(config_.spi_instance);
  }

  /**
   * @brief Load the 74HC165 parallel shift register and read the input data.
   */
  void LoadSR() {
    assert(sr_load_size_ <= sizeof(sr_load_buf_));

    spiAcquireBus(config_.spi_instance);
    palWriteLine(config_.control_pins.latch_load, PAL_LOW);  // HC165 /PL (parallel load) pulse
    chThdSleepMicroseconds(1);
    palWriteLine(config_.control_pins.latch_load, PAL_HIGH);  // HC165 Shift
    palWriteLine(config_.control_pins.inp_cs, PAL_LOW);       // HC165 /CE
    spiReceive(config_.spi_instance, sr_load_size_, sr_load_buf_);
    palWriteLine(config_.control_pins.inp_cs, PAL_HIGH);
    spiReleaseBus(config_.spi_instance);

    // Combine received bytes into a single 16 bit mask
    sr_inp_mask_ = (sr_load_buf_[1] << 8) | sr_load_buf_[0];
  }

  SaboCoverUISeriesInterface* GetSeriesDriver() {
    static systime_t next_process_time = 0;

    if (chVTGetSystemTimeX() < next_process_time) {
      return nullptr;
    }
    next_process_time = chVTGetSystemTimeX() + TIME_S2I(1);  // Process only once per second

    // Set default signals, set all LEDs = 0ff and enable output
    MoboLatchSR(sr_ctrl_mask_ | SR_MASK_S1_LED_ALL_L);
    palWriteLine(config_.control_pins.oe, PAL_LOW);  // Enable HC595 output

    // Read the parallel shift register to get the two /CON signals
    LoadSR();

    // Debug rx_buffer
    auto print_binary = [](uint8_t val, char* buf) {
      for (int i = 7; i >= 0; --i) buf[7 - i] = (val & (1 << i)) ? '1' : '0';
      buf[8] = '\0';
    };
    char rx_buf0_str[9] = {}, rx_buf1_str[9] = {};
    print_binary(sr_load_buf_[0], rx_buf0_str);
    print_binary(sr_load_buf_[1], rx_buf1_str);
    ULOG_INFO("sr_load_buf_: [%s %s]", rx_buf1_str, rx_buf0_str);

    if ((sr_inp_mask_ & INP_MASK_S1_CONNECTED_L) == 0) {
      ULOG_INFO("Detected Series-I CoverUI");
      static SaboCoverUISeries1 series1_driver;
      return &series1_driver;
    }

    if ((sr_inp_mask_ & INP_MASK_S2_CONNECTED_L) == 0) {
      ULOG_INFO("Detected Series-II CoverUI");
      sr_load_size_ = 3;  // Dor Series-II CoverUI we need to read 3 bytes for the HC165
      static SaboCoverUISeries2 series2_driver;
      return &series2_driver;
    }

    // No CoverUI detected
    return nullptr;
  }
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_MOBO_DRIVER_V02_HPP
