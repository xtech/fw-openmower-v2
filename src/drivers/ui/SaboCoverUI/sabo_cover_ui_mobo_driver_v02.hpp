//
// Created by Apehaenger on 6/5/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_MOBO_DRIVER_V02_HPP
#define OPENMOWER_SABO_COVER_UI_MOBO_DRIVER_V02_HPP

#include "sabo_cover_ui_mobo_driver_base.hpp"
#include "sabo_cover_ui_series2.hpp"

namespace xbot::driver::ui {

// Sabo CoverUI Driver for Hardware v0.1
class SaboCoverUIMoboDriverV02 : public SaboCoverUIMoboDriverBase {
 public:
  explicit SaboCoverUIMoboDriverV02(const DriverConfig& config) : SaboCoverUIMoboDriverBase(config) {
  }

  // clang-format off
  // Control bits of 74HC595 shift register
  static constexpr uint8_t SR_MASK_S2_HALL_EN_L     = (1 << 0);  // Series-II Hall /EN
  static constexpr uint8_t SR_MASK_S2_LATCH_LOAD_L  = (1 << 1);  // Series-II Latch- HEF4794BT, /Load 74HC165
  static constexpr uint8_t SR_MASK_S2_OE_L          = (1 << 2);  // Series-II HEF4794BT /OE
  // Series-I LED bits of 74HC595 shift register
  static constexpr uint8_t SR_MASK_S1_LED_AUTO_L    = (1 << 3);
  static constexpr uint8_t SR_MASK_S1_LED_HOME_L    = (1 << 4);
  static constexpr uint8_t SR_MASK_S1_LED_PLAY_RD_L = (1 << 5);
  static constexpr uint8_t SR_MASK_S1_LED_MOW_L     = (1 << 6);
  static constexpr uint8_t SR_MASK_S1_LED_PLAY_GN_L = (1 << 7);
  // clang-format on

  bool Init() override {
    if (!SaboCoverUIMoboDriverBase::Init()) return false;

    uint8_t tx_data = sr_ctrl_mask_ | sr_led_mask_;  // Prepare tx data and merge Ctrl-bits with LED-bits
    uint8_t tx_buf[2] = {tx_data, tx_data};          // Send 2 times the same data
    uint8_t rx_buf[2] = {0, 0};                      // Buffer for SPI RX data

    // Load 74HC165 shift register
    palWriteLine(config_.control_pins.latch_load, PAL_LOW);  // /PL (parallel load) Pulse
    chThdSleepMicroseconds(1);
    palWriteLine(config_.control_pins.latch_load, PAL_HIGH);

    // SPI read/write
    spiAcquireBus(config_.spi_instance);
    palWriteLine(config_.control_pins.inp_cs, PAL_LOW);    // HC165 /CE
    spiExchange(config_.spi_instance, 2, tx_buf, rx_buf);  // Full duplex send and receive
    palWriteLine(config_.control_pins.inp_cs, PAL_HIGH);
    spiReleaseBus(config_.spi_instance);

    sr_inp_mask_ = (rx_buf[1] << 8) | rx_buf[0];

    // Enable output for HC595
    palWriteLine(config_.control_pins.oe, PAL_LOW);

    return true;
  }

  SaboCoverUISeriesInterface* GetSeriesDriver() override {
    // HW v0.2 support CoverUI's of Series-I and II as well as CoverUI-Series-Detection.

    return nullptr;
    // static SaboCoverUISeries2 series2_driver;
    // return &series2_driver;
  }

  // Latch LEDs as well as button-row, and load button columns
  void LatchLoad() override{
      // uint8_t tx_data = current_led_mask_ | series_->GetButtonRowMask();

      // Prepare tx data and merge Ctrl-bits with LED-bits
      /*uint8_t tx_data = sr_ctrlBits.to_ulong() | srLedBits.to_ulong();
      uint8_t rx_data = LatchLoadRaw(tx_data);  // Latch LEDs and load button row

      // Buffer / Shift & buffer depending on current row, as well as advance to next row
      btn_cur_raw_mask_ = series_->ProcessButtonCol(btn_cur_raw_mask_, rx_data);*/
  };

  // Latch tx_data and return loaded data (button columns)
  uint8_t LatchLoadRaw(uint8_t tx_data) {
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
  };

  // Enable output for HEF4794BT
  void EnableOutput() override {
    // TODO: OE could also be used with a PWM signal to dim the LEDs

    // Sabo v0.1 has an HEF4794BT OE driver which inverts the signal. Newer boards will not have this driver anymore.
    palWriteLine(config_.control_pins.oe, PAL_LOW);
  };

 protected:
  // 74HC595 Control bitmask
  uint8_t sr_ctrl_mask_ = SR_MASK_S2_OE_L | SR_MASK_S2_LATCH_LOAD_L;  // = Enable S2-Halls
  // 74HC595 LED bitmask (all off)
  uint8_t sr_led_mask_ = SR_MASK_S1_LED_AUTO_L | SR_MASK_S1_LED_HOME_L | SR_MASK_S1_LED_PLAY_RD_L |
                         SR_MASK_S1_LED_MOW_L | SR_MASK_S1_LED_PLAY_GN_L;
  // 74HC165 input mask
  uint16_t sr_inp_mask_ = 0;

  uint8_t MapLEDIDToBit(LEDID id) const override {
    return series_ ? series_->MapLEDIDToBit(id) : 0;
  };
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_MOBO_DRIVER_V02_HPP
