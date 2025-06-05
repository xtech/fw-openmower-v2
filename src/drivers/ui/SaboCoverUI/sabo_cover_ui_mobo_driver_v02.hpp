//
// Created by Apehaenger on 6/1/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_MOBO_DRIVER_V02_HPP
#define OPENMOWER_SABO_COVER_UI_MOBO_DRIVER_V02_HPP

#include "sabo_cover_ui_mobo_driver_base.hpp"

namespace xbot::driver::ui {

// Sabo CoverUI Driver for Hardware v0.2
class SaboCoverUIMoboDriverV02 : public SaboCoverUIMoboDriverBase {
 public:
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

  void EnableOutput() override {  // Enable output for HEF4794BT
    // TODO: OE could also be used with a PWM signal to dim the LEDs

    // Sabo v0.1 has an HEF4794BT OE driver which inverts the signal. Newer boards will not have this driver anymore.
    palWriteLine(config_.control_pins.oe, PAL_LOW);
  };
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_MOBO_DRIVER_V02_HPP
