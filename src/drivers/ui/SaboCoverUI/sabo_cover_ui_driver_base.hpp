//
// Created by Apehaenger on 5/26/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_DRIVER_BASE_HPP
#define OPENMOWER_SABO_COVER_UI_DRIVER_BASE_HPP

#include "ch.h"
#include "hal.h"
#include "sabo_cover_ui_types.hpp"

namespace xbot::driver::ui {

using sabo::ButtonID;
using sabo::DriverConfig;
using sabo::LEDID;
using sabo::LEDMode;

class SaboCoverUIDriverBase {
 public:
  explicit SaboCoverUIDriverBase(const DriverConfig& config) : config_(config){};

  static constexpr uint8_t DEBOUNCE_TICKS = 40;  // 40 * 1ms(tick) / 2(alternating button rows) = 20ms debounce time

  virtual bool Init();           // Init GPIOs and SPI
  virtual void LatchLoad() = 0;  // Latch data (LEDs, Button-rows, signals) and load inputs (buttons, signals, ...)
  virtual uint8_t LatchLoadRaw(uint8_t tx_data) = 0;  // Do the physical latch & load
  virtual void EnableOutput() = 0;                    // Enable output of HEF4794BT for HW01 or 74HC595 for HW02
  virtual void PowerOnAnimation() = 0;
  virtual bool IsButtonPressed(ButtonID btn) = 0;

  void SetLED(LEDID id, LEDMode mode);  // Set state of a single LED
  void ProcessLedStates();              // Process the different LED modes (on, blink, ...)
  void DebounceButtons();               // Debounce all buttons
  void Tick();                          // Call this function every 1ms to update LEDs, read and debounce buttons, ...

 protected:
  DriverConfig config_;
  SPIConfig spi_cfg_;

  struct LEDState {
    uint8_t on_mask = 0;
    uint8_t slow_blink_mask = 0;
    uint8_t fast_blink_mask = 0;
    systime_t last_slow_update = 0;
    systime_t last_fast_update = 0;
    bool slow_blink_state = false;
    bool fast_blink_state = false;
  };
  LEDState leds_;

  uint8_t current_led_mask_ = 0;  // Current LEDs (with applied LED modes)

  // Map LEDID to bit position. Has to be implemented by derived series base
  virtual uint8_t MapLEDIDToBit(LEDID id) const = 0;

  uint16_t btn_cur_raw_mask_ = 0xFFFF;     // Current read raw button state
  uint16_t btn_last_raw_mask_ = 0xFFFF;    // Last raw button state
  uint16_t btn_stable_raw_mask_ = 0xFFFF;  // Stable (debounced) button state
  uint8_t btn_debounce_counter_ = 0;       // If this counter is >= DEBOUNCE_TICKS, the button state is stable/debounced
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_DRIVER_BASE_HPP
