//
// Created by Apehaenger on 4/19/25.
//

#ifndef OPENMOWER_SABO_UI_DRIVER_HPP
#define OPENMOWER_SABO_UI_DRIVER_HPP

#include "ch.h"
#include "hal.h"

class SaboUIDriver {
 private:
  SPIDriver* spi_;
  SPIConfig spi_cfg_;

  uint8_t current_leds_ = 0;

 public:
  bool init();                 // Init SPI and GPIOs
  void latchLoad();            // Latch LEDs as well as button-row, and load button columns
  void enableOutput();         // Enable output for HEF4794BT
  void setLEDs(uint8_t leds);  // Set LEDs to a specific pattern
};

#endif  // OPENMOWER_SABO_UI_DRIVER_HPP
