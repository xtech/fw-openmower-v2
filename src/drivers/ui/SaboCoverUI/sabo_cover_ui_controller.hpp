//
// Created by Apehaenger on 4/20/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_CONTROLLER_HPP
#define OPENMOWER_SABO_COVER_UI_CONTROLLER_HPP

#include "ch.h"
#include "sabo_cover_ui_cabo_driver_v01.hpp"
#include "sabo_cover_ui_cabo_driver_v02.hpp"
#include "sabo_cover_ui_display.hpp"
#include "sabo_cover_ui_types.hpp"

namespace xbot::driver::ui {

using namespace sabo;

class SaboCoverUIController {
 public:
  void Configure(const CoverUICfg& cui_cfg);  // Configure the controller, select and initialize the driver
  void Start();                               // Starts the controller thread

  bool IsButtonPressed(const ButtonID btn) const;          // Debounced safe check if a specific button is pressed
  static const char* ButtonIDToString(const ButtonID id);  // Get string for ButtonID

 private:
  THD_WORKING_AREA(wa_, 8192);
  thread_t* thread_ = nullptr;

  bool configured_ = false;

  SaboCoverUICaboDriverBase* cabo_ = nullptr;  // Pointer to the Carrierboard driver

  void UpdateStates();  // Update UI state based on system state

  SaboCoverUIDisplay* display_ = nullptr;  // Pointer to the Display driver

  static void ThreadHelper(void* instance);
  void ThreadFunc();
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_CONTROLLER_HPP
