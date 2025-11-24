//
// Created by Apehaenger on 5/26/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_CABO_DRIVER_BASE_HPP
#define OPENMOWER_SABO_COVER_UI_CABO_DRIVER_BASE_HPP

#include "ch.h"
#include "hal.h"
#include "robots/include/sabo_common.hpp"
#include "sabo_cover_ui_defs.hpp"
#include "sabo_cover_ui_series_interface.hpp"

namespace xbot::driver::ui {

class SaboCoverUICaboDriverBase {
 public:
  explicit SaboCoverUICaboDriverBase(const xbot::driver::sabo::config::CoverUi* cover_ui_cfg)
      : cover_ui_cfg_(cover_ui_cfg){};

  // 6 * 10ms(tick) / 2(alternating button rows) = 30ms debounce time
  // Series-I has no alternating button rows, so it will debounce in 60ms (who cares)
  static constexpr uint8_t DEBOUNCE_TICKS = 5;

  virtual bool Init();              // Init GPIOs, SPI and assign series_ driver
  virtual void LatchLoad() = 0;     // Latch data (LEDs, Button-rows, signals) and load inputs (buttons, signals, ...)
  virtual void PowerOnAnimation();  // KIT like anim
  virtual void Tick();              // Call this function every 1ms to update LEDs, read and debounce buttons, ...

  bool IsButtonPressed(ButtonID btn) const;  // Check if a specific button is pressed
  bool IsAnyButtonPressed() const;           // Check if any button is pressed

  /**
   * @brief Get all button states as a standardized bitmask
   * @return Bitmask where bit N is set (1) if button N is pressed, 0 if not pressed
   * Handles conversion from Series-I/II specific button mapping to standardized format
   * For button IDs see xbot::driver::ui::sabo::ButtonID enum
   */
  uint16_t GetButtonsMask() const;

  bool IsReady() const;  // True if CoverUI detected, boot anim played and ready to serve requests

  void SetLED(LEDID id, LEDMode mode);  // Set state of a single LED

  // Debounce all raw buttons at once in one quick XOR operation
  // This Method needs to be called by driver implementation once it read the buttons
  void DebounceRawButtons(uint16_t raw_buttons);

 protected:
  const xbot::driver::sabo::config::CoverUi* cover_ui_cfg_;
  SPIConfig spi_config_;
  SaboCoverUISeriesInterface* series_ = nullptr;  // Series-I/II specific driver

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

  enum class DriverState {
    WAITING_FOR_CUI,  // Waiting for CoverUI to be connected
    BOOT_ANIMATION,   // Bootup- animation started/running
    READY             // Driver is ready to server requests
  };
  DriverState state_ = DriverState::WAITING_FOR_CUI;

  uint8_t current_led_mask_ = 0;  // Series specific current LEDs, with applied LED modes, high-active

  virtual SaboCoverUISeriesInterface* GetSeriesDriver() = 0;  // Get the CoverUI Series driver, if connected
  virtual uint8_t MapLEDIDToMask(LEDID id) const = 0;

  void ProcessLedStates();  // Process the different LED modes (on, blink, ...)

  uint16_t btn_stable_raw_mask_ = 0xFFFF;  // Stable (debounced) button state
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_CABO_DRIVER_BASE_HPP
