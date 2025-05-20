//
// Created by Apehaenger on 4/20/25.
//

#ifndef OPENMOWER_SABO_UI_CONTROLLER_HPP
#define OPENMOWER_SABO_UI_CONTROLLER_HPP

#include "ch.h"
#include "sabo_ui_driver.hpp"

class SaboUIController {
 public:
  explicit SaboUIController(SaboUIDriver* driver) : driver_(driver) {
  }

  static constexpr uint8_t DEBOUNCE_TICKS = 20;  // 20 * 2ms(tick) = 40ms debounce time

  enum class LEDID : uint8_t { AUTO, MOWING, HOME, START_GN, START_RD };  // Same bits as connected to HEF4794BT
  enum class LEDMode { OFF, ON, BLINK_SLOW, BLINK_FAST };
  enum class ButtonID : uint8_t { UP = 0, DOWN, LEFT, RIGHT, OK, START, MENU = 8, BACK, AUTO, MOW, HOME };

  void Start();                         // Initializes the UI driver and starts the controller thread
  void SetLED(LEDID id, LEDMode mode);  // Set LED state

  bool IsButtonPressed(ButtonID btn);  // Debounced safe check if a button is pressed
  // void setButtonCallback(std::function<void(ButtonID)> callback);

  void PlayPowerOnAnimation();

 private:
  THD_WORKING_AREA(wa_, 1024);
  thread_t* thread_ = nullptr;

  SaboUIDriver* driver_;
  // std::function<void(ButtonID)> button_callback_;

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

  bool started_ = false;  // True if the Start() finished

  void HandleLEDModes();   // Handle the different LED modes (on, blink, ...) and apply them to the driver
  void DebounceButtons();  // Debounce all buttons
  void UpdateStates();     // Update UI state based on system state

  static void ThreadHelper(void* instance);
  void ThreadFunc();

  void tick();
};

#endif  // OPENMOWER_SABO_UI_CONTROLLER_HPP
