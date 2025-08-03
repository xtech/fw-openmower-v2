//
// Created by Apehaenger on 4/20/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_CONTROLLER_HPP
#define OPENMOWER_SABO_COVER_UI_CONTROLLER_HPP

#include <etl/array.h>

#include "ch.h"
#include "sabo_cover_ui_cabo_driver_v01.hpp"
#include "sabo_cover_ui_cabo_driver_v02.hpp"
#include "sabo_cover_ui_defs.hpp"
#include "sabo_cover_ui_display.hpp"

namespace xbot::driver::ui {

using namespace xbot::driver::ui::sabo;

class SaboCoverUIController {
 public:
  void Configure(const CoverUICfg& cui_cfg);  // Configure the controller, select and initialize the driver
  void Start();                               // Starts the controller thread

  bool IsButtonPressed(const ButtonID btn) const;          // Debounced safe check if a specific button is pressed
  static const char* ButtonIDToString(const ButtonID id);  // Get string for ButtonID

 private:
  THD_WORKING_AREA(wa_, 5120);  // AH20250801 In use = 3808. Let's be save (+1k) for LVGL GUI development
  thread_t* thread_ = nullptr;

  bool configured_ = false;

  SaboCoverUICaboDriverBase* cabo_ = nullptr;  // Pointer to the Carrierboard driver

  void UpdateStates();  // Update UI state based on system state

  SaboCoverUIDisplay* display_ = nullptr;  // Pointer to the Display driver

  enum class BootStepState { WAIT, RUNNING, ERROR, DONE };
  struct BootStep {
    const char* name;
    bool (*test_func)();
    BootStepState state = BootStepState::WAIT;
    systime_t last_action_time = 0;
  };
  static constexpr size_t BOOT_STEP_COUNT_ = 6;
  etl::array<SaboCoverUIController::BootStep, BOOT_STEP_COUNT_> boot_steps_ = {{
      {"Motion Sensor", &TestIMU},
      {"Charger", &TestCharger},
      {"GPS", &TestGPS},
      {"Left Motor", &TestLeftESC},
      {"Right Motor", &TestRightESC},
      {"Mower Motor", &TestMowerESC},
  }};
  size_t current_boot_step_ = 0;
  static constexpr size_t BOOT_STEP_RETRIES = 3;
  size_t boot_step_retry_count_ = 0;

  void HandleBootSequence();

  static bool TestIMU();
  static bool TestCharger();
  static bool TestGPS();
  static bool TestLeftESC();
  static bool TestRightESC();
  static bool TestMowerESC();

  static void ThreadHelper(void* instance);
  void ThreadFunc();
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_CONTROLLER_HPP
