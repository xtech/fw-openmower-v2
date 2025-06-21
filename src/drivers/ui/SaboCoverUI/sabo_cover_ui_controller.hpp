//
// Created by Apehaenger on 4/20/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_CONTROLLER_HPP
#define OPENMOWER_SABO_COVER_UI_CONTROLLER_HPP

#include "ch.h"
#include "sabo_cover_ui_cabo_driver_v01.hpp"
#include "sabo_cover_ui_cabo_driver_v02.hpp"
#include "sabo_cover_ui_types.hpp"

namespace xbot::driver::ui {

using namespace sabo;

class SaboCoverUIController {
 public:
  void Configure(const DriverConfig& config);  // Configure the controller, select and initialize the driver
  void Start();                                // Starts the controller thread

  bool IsButtonPressed(const ButtonID btn) const;          // Debounced safe check if a specific button is pressed
  static const char* ButtonIDToString(const ButtonID id);  // Get string for ButtonID

 private:
  THD_WORKING_AREA(wa_, 1024);
  thread_t* thread_ = nullptr;

  DriverConfig config_;                          // Configuration for the CoverUI driver
  SaboCoverUICaboDriverBase* driver_ = nullptr;  // Pointer to the UI driver

  bool configured_ = false;
  bool started_ = false;  // True if the Start() finished

  void UpdateStates();  // Update UI state based on system state

  static void ThreadHelper(void* instance);
  void ThreadFunc();
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_CONTROLLER_HPP
