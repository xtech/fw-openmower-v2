//
// Created by Apehaenger on 4/19/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_DRIVER_HPP
#define OPENMOWER_SABO_COVER_UI_DRIVER_HPP

#include "ch.h"
#include "hal.h"

class SaboCoverUIDriver {
 private:
  SPIDriver* spi_;
  SPIConfig spi_cfg_;

  uint8_t current_leds_ = 0;
  uint8_t current_button_row_ = 0;  // Alternating button rows
  uint16_t button_states_ = 0;      // Bits 0-7: Row0, Bits 8-15: Row1. Low-active!

 public:
  bool Init();                          // Init SPI and GPIOs
  void LatchLoad();                     // Latch LEDs as well as button-row, and load button columns
  void EnableOutput();                  // Enable output for HEF4794BT
  uint16_t GetRawButtonStates() const;  // Get the raw button states (0-15). Low-active!
  void SetLEDs(uint8_t leds);           // Set LEDs to a specific pattern
};

#endif  // OPENMOWER_SABO_COVER_UI_DRIVER_HPP
