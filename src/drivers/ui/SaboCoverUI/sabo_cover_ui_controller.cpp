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

#include <cstdint>
#include <globals.hpp>
#include <services.hpp>

#include "sabo_cover_ui_display.hpp"

namespace xbot::driver::ui {

using namespace xbot::driver::sabo::types;

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
        etl::delegate<bool(ButtonId)>::create<SaboCoverUIController, &SaboCoverUIController::IsButtonPressed>(*this));
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
  thread_->name = "SaboCoverUIController";
#endif
}

void SaboCoverUIController::ThreadHelper(void* instance) {
  auto* i = static_cast<SaboCoverUIController*>(instance);
  i->ThreadFunc();
}

void SaboCoverUIController::UpdateStates() {
  if (!cabo_->IsReady()) return;

  // Play LEDs
  // For identification purposes, PLAY_RD get handled exclusively with higher priority before PLAY_GN
  if (emergency_service.GetEmergencyReasons() != 0) {
    cabo_->SetLed(LedId::PLAY_RD, LedMode::BLINK_FAST);  // Emergency
    cabo_->SetLed(LedId::PLAY_GN, LedMode::OFF);
  } else if (emergency_service.GetEmergencyReasons() & EmergencyReason::TIMEOUT_HIGH_LEVEL) {  // ROS not connected
    cabo_->SetLed(LedId::PLAY_RD, LedMode::BLINK_SLOW);
    cabo_->SetLed(LedId::PLAY_GN, LedMode::OFF);
  } else {
    cabo_->SetLed(LedId::PLAY_RD, LedMode::OFF);

    // PLAY_GN LED handling
    if (power_service.GetAdapterVolts() > 26.0f) {    // Docked
      if (power_service.GetBatteryVolts() < 20.0f) {  // No (or dead) battery
        cabo_->SetLed(LedId::PLAY_GN, LedMode::BLINK_FAST);
      } else if (power_service.GetChargeCurrent() > 0.1f) {  // Battery charging
        cabo_->SetLed(LedId::PLAY_GN, LedMode::BLINK_SLOW);
      } else {  // Battery charged
        cabo_->SetLed(LedId::PLAY_GN, LedMode::ON);
      }
    } else {
      cabo_->SetLed(LedId::PLAY_GN, LedMode::OFF);
    }
  }

  // Handle HighLevelService states for AUTO, MOWING, HOME LEDs
  const HighLevelStatus hl_status = high_level_service.GetStateId();
  const uint8_t SUBSTATE_SHIFT = static_cast<uint8_t>(HighLevelStatus::SUBSTATE_SHIFT);
  const uint8_t STATE_MASK = (1 << SUBSTATE_SHIFT) - 1;
  const uint8_t SUBSTATE_MASK = static_cast<uint8_t>(HighLevelStatus::SUBSTATE_MASK);

  uint8_t hl_status_raw = static_cast<uint8_t>(hl_status);
  HighLevelStatus main_state = static_cast<HighLevelStatus>(hl_status_raw & STATE_MASK);
  HighLevelStatus sub_state = static_cast<HighLevelStatus>((hl_status_raw >> SUBSTATE_SHIFT) & SUBSTATE_MASK);

  // AUTO LED
  if (main_state == HighLevelStatus::AUTONOMOUS) {
    cabo_->SetLed(LedId::AUTO, LedMode::ON);
  } else if (power_service.GetAdapterVolts() <= 26.0f) {
    cabo_->SetLed(LedId::AUTO, LedMode::BLINK_SLOW);  // Hanging around indicator
  } else {
    cabo_->SetLed(LedId::AUTO, LedMode::OFF);
  }

  // MOWING LED
  if (main_state == HighLevelStatus::AUTONOMOUS && sub_state == HighLevelStatus::SUBSTATE_1) {
    if (high_level_service.GetStateName().compare("MOWING") == 0) {
      cabo_->SetLed(LedId::MOWING, LedMode::ON);
    } else {
      cabo_->SetLed(LedId::MOWING, LedMode::BLINK_SLOW);
    }
  } else {
    cabo_->SetLed(LedId::MOWING, LedMode::OFF);
  }

  // HOME (Docking) LED
  if (main_state == HighLevelStatus::AUTONOMOUS && sub_state == HighLevelStatus::SUBSTATE_2) {
    cabo_->SetLed(LedId::HOME, LedMode::ON);
  } else {
    cabo_->SetLed(LedId::HOME, LedMode::OFF);
  }
}

bool SaboCoverUIController::IsButtonPressed(const ButtonId button) const {
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

  // Next wakeup time for a more precise 10ms loop interval
  systime_t next_wakeup;

  static systime_t last_button_check = 0;
  static systime_t last_display_tick = 0;

  while (true) {
    // Set next wakeup time to absolute current time + 10ms
    next_wakeup = chVTGetSystemTimeX() + TIME_MS2I(10);

    const systime_t now = chVTGetSystemTimeX();

    // Update CABO driver
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
      for (size_t i = 0; i < defs::NUM_BUTTONS; ++i) {
        const auto& button_id = defs::ALL_BUTTONS[i];
        bool is_pressed = cabo_->IsButtonPressed(button_id);

        // Only trigger on rising edge (button was not pressed before, but is pressed now)
        if (is_pressed && !button_states_[i]) {
          button_states_[i] = true;
          ULOG_INFO("Sabo CoverUI Button [%s] pressed", ButtonIdToString(button_id));

          // Let the active screen handle the button first
          if (display_->OnButtonPress(button_id)) {
            continue;  // Handled by screen
          }

          // If display_ (and thus also screens) didn't handle buttons, global button logic could also apply here
        } else if (!is_pressed && button_states_[i]) {
          // Button released - reset state
          button_states_[i] = false;
        }
      }
    }

    // chThdSleepUntil is not past save
    if (chVTGetSystemTimeX() + TIME_US2I(10) < next_wakeup) {
      // Sleep until next 10ms interval
      chThdSleepUntil(next_wakeup);
    }
  }
}

}  // namespace xbot::driver::ui
