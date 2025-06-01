//
// Created by Apehaenger on 4/20/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_CONTROLLER_HPP
#define OPENMOWER_SABO_COVER_UI_CONTROLLER_HPP

#include "ch.h"
#include "sabo_cover_ui_driver_s2_hw01.hpp"
#include "sabo_cover_ui_types.hpp"

namespace xbot::driver::ui {

using sabo::ButtonID;
using sabo::DriverConfig;
using sabo::LEDID;
using sabo::LEDMode;

class SaboCoverUIController {
 public:
  void Configure(const DriverConfig& config);  // Configure the controller, select and initialize the driver
  void Start();                                // Starts the controller thread

  bool IsButtonPressed(ButtonID btn);  // Debounced safe check if a button is pressed

 private:
  THD_WORKING_AREA(wa_, 1024);
  thread_t* thread_ = nullptr;

  DriverConfig config_;                      // Configuration for the CoverUI driver
  SaboCoverUIDriverBase* driver_ = nullptr;  // Pointer to the UI driver

  bool configured_ = false;
  bool started_ = false;  // True if the Start() finished

  void UpdateStates();  // Update UI state based on system state

  static void ThreadHelper(void* instance);
  void ThreadFunc();
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_CONTROLLER_HPP
