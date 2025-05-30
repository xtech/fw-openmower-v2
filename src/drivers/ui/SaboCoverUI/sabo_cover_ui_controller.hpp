//
// Created by Apehaenger on 4/20/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_CONTROLLER_HPP
#define OPENMOWER_SABO_COVER_UI_CONTROLLER_HPP

#include "ch.h"
#include "sabo_cover_ui_driver_s2_hw01.hpp"

namespace xbot::driver::ui {

class SaboCoverUIController {
 public:
  SaboCoverUIController() = default;

  static constexpr uint8_t DEBOUNCE_TICKS = 20;  // 20 * 2ms(tick) = 40ms debounce time

  // Same bit order as in CoverUI Series-II (HEF4794BT). Series-I driver need to map the bits
  enum class LEDID : uint8_t { AUTO = 0, MOWING, HOME, START_GN, START_RD };
  enum class LEDMode { OFF, ON, BLINK_SLOW, BLINK_FAST };
  // Same bit order as in CoverUI Series-II (74HC165, Row 0/1). Series-I driver need to translate the bits
  enum class ButtonID : uint8_t { UP = 0, DOWN, LEFT, RIGHT, OK, START, MENU = 8, BACK, AUTO, MOW, HOME };

  void Configure(const SaboDriverConfig& config);  // Configure the controller, select and initialize the driver
  void Start();                                    // Starts the controller thread
  void SetLED(LEDID id, LEDMode mode);             // Set LED state

  bool IsButtonPressed(ButtonID btn);  // Debounced safe check if a button is pressed

  void PlayPowerOnAnimation();

 private:
  THD_WORKING_AREA(wa_, 1024);
  thread_t* thread_ = nullptr;

  SaboDriverConfig config_;                  // Configuration for the CoverUI driver
  SaboCoverUIDriverBase* driver_ = nullptr;  // Pointer to the UI driver

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

  uint16_t btn_last_raw_ = 0xFFFF;       // Last raw button state
  uint16_t btn_stable_states_ = 0xFFFF;  // Stable (debounced) button state
  uint8_t btn_debounce_counter_ = 0;     // If this counter is >= DEBOUNCE_TICKS, the button state is stable/debounced

  bool configured_ = false;
  bool started_ = false;  // True if the Start() finished

  void HandleLEDModes();   // Handle the different LED modes (on, blink, ...) and apply them to the driver
  void DebounceButtons();  // Debounce all buttons
  void UpdateStates();     // Update UI state based on system state

  static void ThreadHelper(void* instance);
  void ThreadFunc();
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_CONTROLLER_HPP
