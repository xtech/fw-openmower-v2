/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sabo_cover_ui_controller.cpp
 * @brief Controller for the Sabo Cover UI
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-04-20
 */

#include "sabo_cover_ui_controller.hpp"

#include <ulog.h>

#include <globals.hpp>
#include <services.hpp>

#include "sabo_cover_ui_display.hpp"

namespace xbot::driver::ui {

using namespace xbot::driver::ui::sabo;
using namespace xbot::driver::sabo;

SaboCoverUIController::SaboCoverUIController(const config::HardwareConfig& hardware_config) {
  // Select the CoverUI driver based on the hardware configuration
  if (hardware_config.cover_ui != nullptr) {
    // Select driver based on hardware version
    if (GetHardwareVersion(carrier_board_info) == types::HardwareVersion::V0_1) {
      // Mobo v0.1 has only CoverUI-Series-II support and no CoverUI-Series detection
      static SaboCoverUICaboDriverV01 driver_v01(hardware_config.cover_ui);
      cabo_ = &driver_v01;
    } else {
      // Mobo v0.2 and later support both CoverUI-Series (I & II) as well as it has CoverUI-Series detection
      static SaboCoverUICaboDriverV02 driver_v02(hardware_config.cover_ui);
      cabo_ = &driver_v02;
    }
  }

  // Initialize display if available
  if (hardware_config.lcd != nullptr) {
    static SaboCoverUIDisplay display(
        hardware_config.lcd,
        etl::delegate<bool(ButtonID)>::create<SaboCoverUIController, &SaboCoverUIController::IsButtonPressed>(*this));
    display_ = &display;
  }
}

void SaboCoverUIController::Start() {
  if (thread_ != nullptr) {
    ULOG_ERROR("Sabo CoverUI Controller started twice!");
    return;
  }

  if (cabo_ == nullptr) {
    ULOG_ERROR("Sabo CoverUI Driver not set!");
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

uint16_t SaboCoverUIController::GetButtonsMask() const {
  if (!cabo_ || !cabo_->IsReady()) return 0;
  return cabo_->GetButtonsMask();
}

void SaboCoverUIController::ThreadFunc() {
  if (display_) {
    display_->Start();
    display_->WakeUp();
  }

  while (true) {
    static uint32_t last_button_check = 0;
    static uint32_t last_display_tick = 0;
    uint32_t now = chVTGetSystemTimeX();

    cabo_->Tick();
    UpdateStates();

    if (cabo_->IsReady() && display_) {
      // 30ms tick for display is quite enough and will save resources
      if (TIME_I2MS(now - last_display_tick) >= 30) {
        last_display_tick = now;
        display_->Tick();
      }
      if (cabo_->IsAnyButtonPressed()) display_->WakeUp();
    }

    // ----- Button Handling -----
    if (TIME_I2MS(now - last_button_check) >= 100) {  // Check buttons every 100ms
      last_button_check = now;

      // Iterate over all valid buttons using the companion array
      for (const auto& button_id : ALL_BUTTONS) {
        size_t btn_index = static_cast<size_t>(button_id);
        bool is_pressed = cabo_->IsButtonPressed(button_id);

        // Only trigger on rising edge (button was not pressed before, but is pressed now)
        if (is_pressed && !button_states_[btn_index]) {
          button_states_[btn_index] = true;
          ULOG_INFO("Sabo CoverUI Button [%s] pressed", ButtonIDToString(button_id));

          // Let the active screen handle the button first
          if (display_->OnButtonPress(button_id)) {
            continue;  // Handled by screen
          }

          // If display_ (and thus also screens) didn't handle buttons, global button logic could also apply here
        } else if (!is_pressed && button_states_[btn_index]) {
          // Button released - reset state
          button_states_[btn_index] = false;
        }
      }
    }

    // Sleep max. 10ms for reliable button debouncing
    chThdSleep(TIME_MS2I(10));
  }
}

}  // namespace xbot::driver::ui
