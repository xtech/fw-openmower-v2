//
// Created by Apehaenger on 4/19/25.
//
#include "sabo_cover_ui_driver_s2_hw01.hpp"

#include <ulog.h>

#include "../robots/src/Sabo/robot_ex.hpp"
#include "globals.hpp"

namespace xbot::driver::ui {

bool SaboCoverUIDriverS2HW01::Init() {
  // Init SPI in base class
  if (!SaboCoverUIDriverBase::Init()) {
    return false;
  }

  // Init GPIOs for chip control
  palSetLineMode(config_.control_pins.latch_load,
                 PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_FLOATING);
  palSetLineMode(config_.control_pins.oe, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_FLOATING);
  palSetLineMode(config_.control_pins.btn_cs,
                 PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_FLOATING);

  return true;
}

void SaboCoverUIDriverS2HW01::LatchLoad() {
  uint8_t button_data = 0;
  uint8_t tx_data = current_leds_ | ((current_button_row_ + 1) << 5);  // Bit5/6 for row0/1

  // Load 74HC165 shift register (the very first load will load nothing, because row is not set yet. Who cares)
  palWriteLine(config_.control_pins.latch_load, PAL_LOW);  // /PL (parallel load) Pulse
  chThdSleepMicroseconds(1);
  palWriteLine(config_.control_pins.latch_load, PAL_HIGH);

  // SPI transfer LEDs and read button for previously set row
  spiAcquireBus(config_.spi_instance);
  palWriteLine(config_.control_pins.latch_load, PAL_HIGH);       // Latch HEF4794BT (STR = HIGH)
  palWriteLine(config_.control_pins.btn_cs, PAL_LOW);            // HC165 /CE
  spiExchange(config_.spi_instance, 1, &tx_data, &button_data);  // Send data and receive button data
  palWriteLine(config_.control_pins.latch_load, PAL_LOW);        // Stop latching (STR = LOW)
  palWriteLine(config_.control_pins.btn_cs, PAL_HIGH);
  spiReleaseBus(config_.spi_instance);

  // Buffer / Shift & buffer depending on current row
  if (current_button_row_ == 0) {
    button_states_ = (button_states_ & 0xFF00) | button_data;
  } else {
    button_states_ = (button_states_ & 0x00FF) | (button_data << 8);
  }

  // Alternate between row0 and row1
  current_button_row_ ^= 1;
}

void SaboCoverUIDriverS2HW01::EnableOutput() {
  // TODO: OE could also be used with a PWM signal to dim the LEDs

  if (carrier_board_info.version_major <= 0 && carrier_board_info.version_minor <= 1) {
    // Sabo v0.1 has an HEF4794BT OE driver which inverts the signal. Newer boards will not have this driver anymore.
    palWriteLine(config_.control_pins.oe, PAL_LOW);
  } else {
    palWriteLine(config_.control_pins.btn_cs, PAL_HIGH);
  }
}

void SaboCoverUIDriverS2HW01::SetLEDs(uint8_t leds) {
  current_leds_ = leds & 0x1F;  // LEDs are on the lower 5 bits
}

uint16_t SaboCoverUIDriverS2HW01::GetRawButtonStates() const {
  return button_states_;
}

}  // namespace xbot::driver::ui
