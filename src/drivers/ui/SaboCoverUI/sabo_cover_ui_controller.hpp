//
// Created by Apehaenger on 4/20/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_CONTROLLER_HPP
#define OPENMOWER_SABO_COVER_UI_CONTROLLER_HPP

#include <etl/array.h>

#include "ch.h"
#include "robots/include/sabo_common.hpp"
#include "sabo_cover_ui_cabo_driver_v01.hpp"
#include "sabo_cover_ui_cabo_driver_v02.hpp"
#include "sabo_cover_ui_defs.hpp"

namespace xbot::driver::ui {

// Forward declaration to avoid circular include
class SaboCoverUIDisplay;

using namespace xbot::driver::sabo;
using namespace xbot::driver::ui::sabo;

class SaboCoverUIController {
 public:
  explicit SaboCoverUIController(const config::HardwareConfig& hardware_config);

  void Start();

  bool IsButtonPressed(const ButtonID btn) const;  // Debounced safe check if a specific button is pressed
  uint16_t GetButtonsMask() const;                 // Returns bitmask of current button states

 private:
  THD_WORKING_AREA(wa_, 5120);  // AH20251110 In use = 4416. Let's be save (+~1k) for LVGL GUI development
  thread_t* thread_ = nullptr;

  SaboCoverUICaboDriverBase* cabo_ = nullptr;  // Pointer to the Carrierboard driver

  void UpdateStates();  // Update UI state based on system state

  SaboCoverUIDisplay* display_ = nullptr;  // Pointer to the Display driver

  // Button debouncing state - array indexed by ButtonID value (has gap at index 7)
  etl::array<bool, static_cast<size_t>(ButtonID::_LAST) + 1> button_states_{};

  static void ThreadHelper(void* instance);
  void ThreadFunc();
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_CONTROLLER_HPP
