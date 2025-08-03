//
// Created by Apehaenger on 4/20/25.
//
#include "sabo_cover_ui_controller.hpp"

#include <ulog.h>

#include <services.hpp>

#include "chprintf.h"
#include "globals.hpp"
#include "hal.h"
#include "robots/include/sabo_robot.hpp"
#include "sabo_cover_ui_display.hpp"
#include "sabo_cover_ui_display_driver_uc1698.hpp"

namespace xbot::driver::ui {

using namespace xbot::driver::ui::sabo;

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

  if (display_) {
    if (!display_->Init()) {
      ULOG_ERROR("Sabo CoverUI Display initialization failed!");
      return;
    }
  }

  thread_ = chThdCreateStatic(&wa_, sizeof(wa_), NORMALPRIO, ThreadHelper, this);
#ifdef USE_SEGGER_SYSTEMVIEW
  processing_thread_->name = "SaboCoverUIController";
#endif
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

/**
 * @brief Handle the boot sequence
 *
 */
void SaboCoverUIController::HandleBootSequence() {
  auto boot_screen = display_->GetBootScreen();
  if (boot_screen == nullptr) return;

  // All boot steps done...
  if (current_boot_step_ >= boot_steps_.size()) {
    if (boot_screen->GetAnimationState() == SaboScreenBoot::AnimationState::NONE) {
      display_->SetBootStatus("Ready to mow", 100);
      boot_screen->StartForwardAnimation();
    } else if (boot_screen->GetAnimationState() == SaboScreenBoot::AnimationState::DONE) {
      display_->ShowMainScreen();
    }
    return;
  }

  BootStep& step = boot_steps_[current_boot_step_];
  systime_t now = chVTGetSystemTimeX();
  uint32_t elapsed_ms = TIME_I2MS(now - step.last_action_time);

  switch (step.state) {
    case BootStepState::WAIT:
      step.state = BootStepState::RUNNING;
      step.last_action_time = now;
      display_->SetBootStatus(step.name, (current_boot_step_ * 100) / boot_steps_.size());
      break;

    case BootStepState::RUNNING:
      if (elapsed_ms >= 1000) {
        if (step.test_func()) {
          step.state = BootStepState::DONE;
          current_boot_step_++;
          boot_step_retry_count_ = 0;
        } else {
          boot_step_retry_count_++;
          if (boot_step_retry_count_ < BOOT_STEP_RETRIES) {
            etl::string<48> fail_msg;
            chsnprintf(fail_msg.data(), fail_msg.max_size(), "%s (%d/%d)", step.name, boot_step_retry_count_,
                       BOOT_STEP_RETRIES);
            display_->SetBootStatus(fail_msg, (current_boot_step_ * 100) / boot_steps_.size());
            step.last_action_time = now;
          } else {
            etl::string<48> fail_msg = step.name;
            fail_msg += " failed!";
            display_->SetBootStatus(fail_msg, (current_boot_step_ * 100) / boot_steps_.size());
            step.state = BootStepState::ERROR;
            step.last_action_time = now;
          }
        }
      }
      break;

    case BootStepState::ERROR:
      if (elapsed_ms >= 60000 || cabo_->IsAnyButtonPressed()) {
        step.state = BootStepState::DONE;
        current_boot_step_++;
        boot_step_retry_count_ = 0;
      }
      break;

    case BootStepState::DONE:
      // NOP. Next step get handled in next tick
      break;
  }
}

bool SaboCoverUIController::TestIMU() {
  return imu_service.IsFound();
}

bool SaboCoverUIController::TestCharger() {
  Robot* robot = GetRobot();
  if (!robot) return false;

  auto* sabo = static_cast<SaboRobot*>(robot);
  if (!sabo) return false;

  const CHARGER_STATUS status = sabo->GetChargerStatus();
  // TODO: Why do I get COMMS_ERROR here?
  // return status != CHARGER_STATUS::FAULT && status != CHARGER_STATUS::COMMS_ERROR;
  return status != CHARGER_STATUS::FAULT;
}

bool SaboCoverUIController::TestGPS() {
  return true;
}

bool SaboCoverUIController::TestLeftESC() {
  if (!robot) return false;
  auto* sabo = static_cast<SaboRobot*>(robot);
  return sabo->TestLeftESC();
}

bool SaboCoverUIController::TestRightESC() {
  if (!robot) return false;
  auto* sabo = static_cast<SaboRobot*>(robot);
  return sabo->TestRightESC();
}

bool SaboCoverUIController::TestMowerESC() {
  if (!robot) return false;
  auto* sabo = static_cast<SaboRobot*>(robot);
  return sabo->TestMowerESC();
}

void SaboCoverUIController::ThreadFunc() {
  if (display_) {
    display_->Start();
    display_->WakeUp();
  }

  while (true) {
    cabo_->Tick();
    UpdateStates();
    if (cabo_->IsReady() && display_) {
      display_->Tick();
      if (cabo_->IsAnyButtonPressed()) display_->WakeUp();

      if (current_boot_step_ <= boot_steps_.size() && display_->GetActiveScreen()->GetScreenId() == ScreenId::BOOT) {
        HandleBootSequence();
      }
    }

    // ----- Debug -----
    static uint32_t last_debug_time = 0;
    uint32_t now = chVTGetSystemTimeX();
    if (TIME_I2MS(now - last_debug_time) >= 500) {
      last_debug_time = now;
      for (int btn = static_cast<int>(ButtonID::_FIRST); btn <= static_cast<int>(ButtonID::_LAST); ++btn) {
        ButtonID button = static_cast<ButtonID>(btn);
        if (cabo_->IsButtonPressed(button)) {
          ULOG_INFO("Sabo CoverUI Button [%s] pressed", ButtonIDToString(static_cast<ButtonID>(btn)));
        }
      }
    }

    // Sleep max. 10ms for reliable button debouncing
    chThdSleep(TIME_MS2I(10));
  }
}

}  // namespace xbot::driver::ui
