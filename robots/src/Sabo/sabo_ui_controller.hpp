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

  enum class LEDID { AUTO, MOWING, HOME, START_GN, START_RD };  // Same bits as connected to HEF4794BT
  enum class LEDMode { OFF, ON, BLINK_SLOW, BLINK_FAST };
  enum class ButtonID { AUTO, MOW, HOME, MODE, START, BACK, MENU, UP, DOWN, LEFT, RIGHT, OK };

  void start();  // Initializes the UI driver and starts the controller thread
  void setLED(LEDID id, LEDMode mode);

  bool isButtonPressed(ButtonID id);
  // void setButtonCallback(std::function<void(ButtonID)> callback);

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

  static void ThreadHelper(void* instance);
  void ThreadFunc();

  void tick();
};

#endif  // OPENMOWER_SABO_UI_CONTROLLER_HPP
