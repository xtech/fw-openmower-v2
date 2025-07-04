//
// Created by Apehaenger on 4/20/25.
//
#include "sabo_cover_ui_controller.hpp"

#include <ulog.h>

#include <services.hpp>

#include "hal.h"
#include "sabo_cover_ui_display.hpp"
#include "sabo_cover_ui_display_driver_uc1698.hpp"

namespace xbot::driver::ui {

using namespace sabo;

void SaboCoverUIController::Configure(const CoverUICfg& cui_cfg) {
  // Select the CoverUI driver based on the carrier board version and/or CoverUI Series
  if (carrier_board_info.version_major == 0 && carrier_board_info.version_minor == 1) {
    // Mobo v0.1 has only CoverUI-Series-II support and no CoverUI-Series detection
    static SaboCoverUICaboDriverV01 driver_v01(cui_cfg.cabo_cfg);
    cabo_ = &driver_v01;
  } else {
    // Mobo v0.2 and later support both CoverUI-Series (I & II) as well as it has CoverUI-Series detection
    static SaboCoverUICaboDriverV02 driver_v02(cui_cfg.cabo_cfg);
    cabo_ = &driver_v02;
    static SaboCoverUIDisplay display(cui_cfg.lcd_cfg);
    display_ = &display;
  }

  configured_ = true;
}

void SaboCoverUIController::Start() {
  if (thread_ != nullptr) {
    ULOG_ERROR("Sabo CoverUI Controller started twice!");
    return;
  }

  if (!configured_) {
    ULOG_ERROR("Sabo CoverUI Controller not configured!");
    return;
  }

  if (cabo_ == nullptr) {
    ULOG_ERROR("Sabo CoverUI Driver not set!");
    return;
  }
  if (!cabo_->Init()) {
    ULOG_ERROR("Sabo CoverUI Driver initialization failed!");
    return;
  }

  thread_ = chThdCreateStatic(&wa_, sizeof(wa_), NORMALPRIO, ThreadHelper, this);
#ifdef USE_SEGGER_SYSTEMVIEW
  processing_thread_->name = "SaboCoverUIController";
#endif

  if (display_) {
    if (!display_->Init()) {
      ULOG_ERROR("Sabo CoverUI Display initialization failed!");
      return;
    }
    display_->Start();
  }
}

void SaboCoverUIController::ThreadHelper(void* instance) {
  auto* i = static_cast<SaboCoverUIController*>(instance);
  i->ThreadFunc();
}

void SaboCoverUIController::UpdateStates() {
  if (!cabo_->IsReady()) return;

  // const auto high_level_state = mower_ui_service.GetHighLevelState();

  // Start LEDs
  // For identification purposes, Red-Start-LED get handled exclusively with high priority before Green-Start-LED
  if (emergency_service.GetEmergency()) {
    cabo_->SetLED(LEDID::PLAY_RD, LEDMode::BLINK_FAST);  // Emergency
    cabo_->SetLED(LEDID::PLAY_GN, LEDMode::OFF);
    /* FIXME: Add/Enable once mower_ui_service is working
  } else if (high_level_state ==
             MowerUiService::HighLevelState::MODE_UNKNOWN) { // Waiting for ROS
    cabo_->SetLED(LEDID::PLAY_RD, LEDMode::BLINK_SLOW);              */
    cabo_->SetLED(LEDID::PLAY_GN, LEDMode::OFF);
  } else {
    cabo_->SetLED(LEDID::PLAY_RD, LEDMode::OFF);

    // Green-Start-LED
    if (power_service.GetAdapterVolts() > 26.0f) {    // Docked
      if (power_service.GetBatteryVolts() < 20.0f) {  // No (or dead) battery
        cabo_->SetLED(LEDID::PLAY_GN, LEDMode::BLINK_FAST);
      } else if (power_service.GetChargeCurrent() > 0.1f) {  // Battery charging
        cabo_->SetLED(LEDID::PLAY_GN, LEDMode::BLINK_SLOW);
      } else {  // Battery charged
        cabo_->SetLED(LEDID::PLAY_GN, LEDMode::ON);
      }
    } else {
      // TODO: Handle high level states like "Mowing" or "Area Recording"
      cabo_->SetLED(LEDID::PLAY_GN, LEDMode::OFF);
    }
  }
}

bool SaboCoverUIController::IsButtonPressed(const ButtonID button) const {
  if (!cabo_->IsReady()) return false;
  return cabo_->IsButtonPressed(button);
}

const char* SaboCoverUIController::ButtonIDToString(const ButtonID id) {
  switch (id) {
    case ButtonID::UP: return "Up";
    case ButtonID::DOWN: return "Down";
    case ButtonID::LEFT: return "Left";
    case ButtonID::RIGHT: return "Right";
    case ButtonID::OK: return "OK";
    case ButtonID::PLAY: return "Play";
    case ButtonID::S1_SELECT: return "Select (S1)";
    case ButtonID::MENU: return "Menu";
    case ButtonID::BACK: return "Back";
    case ButtonID::S2_AUTO: return "Auto (S2)";
    case ButtonID::S2_MOW: return "Mow (S2)";
    case ButtonID::S2_HOME: return "Home (S2)";
    default: return "Unknown";
  }
}

void SaboCoverUIController::ThreadFunc() {
  // Now that the LCD got started (thread running and fully initialized), wake it up
  if (display_) display_->WakeUp();

  while (true) {
    cabo_->Tick();
    UpdateStates();
    if (display_) display_->Tick();

    // ----- Debug -----
    static uint32_t last_debug_time = 0;
    uint32_t now = chVTGetSystemTimeX();
    if (TIME_I2MS(now - last_debug_time) >= 500) {
      last_debug_time = now;
      for (int btn = static_cast<int>(ButtonID::_FIRST); btn <= static_cast<int>(ButtonID::_LAST); ++btn) {
        ButtonID button = static_cast<ButtonID>(btn);
        if (cabo_->IsButtonPressed(button)) {
          ULOG_INFO("Button [%s] pressed", ButtonIDToString(static_cast<ButtonID>(btn)));
        }
      }
    }

    // Sleep max. 10ms for reliable button debouncing
    chThdSleep(TIME_MS2I(10));
  }
}

}  // namespace xbot::driver::ui
