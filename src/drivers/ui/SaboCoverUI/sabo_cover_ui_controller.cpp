//
// Created by Apehaenger on 4/20/25.
//
#include "sabo_cover_ui_controller.hpp"

#include <ulog.h>

#include <services.hpp>

namespace xbot::driver::ui {

using sabo::DriverConfig;

void SaboCoverUIController::Configure(const DriverConfig& config) {
  config_ = config;
  configured_ = true;

  // Select the CoverUI driver based on the carrier board version and/or CoverUI Series
  if (carrier_board_info.version_major == 0 && carrier_board_info.version_minor == 1) {
    // HW v0.1 has only CoverUI-Series-II support and no CoverUI-Series detection
    static SaboCoverUIDriverS2HW01 driver_s2hw01(config);
    driver_ = &driver_s2hw01;
  } else {
    // HW v0.2 and later support both CoverUI-Series (I & II) as well as it has CoverUI-Series detection
    // TODO: Try drivers if connected
  }
}

void SaboCoverUIController::Start() {
  if (thread_ != nullptr) {
    ULOG_ERROR("Started Sabo CoverUI Controller twice!");
    return;
  }

  if (!configured_) {
    ULOG_ERROR("Sabo CoverUI Controller not configured!");
    return;
  }

  if (driver_ == nullptr) {
    ULOG_ERROR("Sabo CoverUI Driver not set!");
    return;
  }
  if (!driver_->Init()) {
    ULOG_ERROR("Failed to initialize Sabo CoverUI Driver!");
    return;
  }

  thread_ = chThdCreateStatic(&wa_, sizeof(wa_), NORMALPRIO, ThreadHelper, this);
#ifdef USE_SEGGER_SYSTEMVIEW
  processing_thread_->name = "SaboCoverUIController";
#endif

  // Now that driver is initialized and thread got started, we can enable output
  driver_->EnableOutput();
  chThdSleepMilliseconds(100);
  driver_->PowerOnAnimation();
  chThdSleepMilliseconds(500);
  started_ = true;
}

void SaboCoverUIController::ThreadHelper(void* instance) {
  auto* i = static_cast<SaboCoverUIController*>(instance);
  i->ThreadFunc();
}

void SaboCoverUIController::DebounceButtons() {
  // Debounce all buttons (at once)
  const uint16_t raw = driver_->GetRawButtonStates();
  const uint16_t changed_bits = btn_last_raw_ ^ raw;  // XOR to find changed bits
  if (changed_bits == 0) {
    if (btn_debounce_counter_ < DEBOUNCE_TICKS) btn_debounce_counter_++;
  } else {
    btn_debounce_counter_ = 0;
  }
  btn_stable_states_ = (btn_debounce_counter_ >= DEBOUNCE_TICKS) ? raw : btn_stable_states_;
  btn_last_raw_ = raw;
}

void SaboCoverUIController::UpdateStates() {
  if (!started_) return;

  // const auto high_level_state = mower_ui_service.GetHighLevelState();

  // Start LEDs
  // For identification purposes, Red-Start-LED get handled exclusively with high priority before Green-Start-LED
  if (emergency_service.GetEmergency()) {
    driver_->SetLED(LEDID::START_RD, LEDMode::BLINK_FAST);  // Emergency
    /* FIXME: Add/Enable once mower_ui_service is working
  } else if (high_level_state ==
             MowerUiService::HighLevelState::MODE_UNKNOWN) {
    driver_->SetLED(LEDID::START_RD, LEDMode::BLINK_SLOW);             // Waiting for ROS */
  } else {
    driver_->SetLED(LEDID::START_RD, LEDMode::OFF);

    // Green-Start-LED
    if (power_service.GetAdapterVolts() > 26.0f) {    // Docked
      if (power_service.GetBatteryVolts() < 20.0f) {  // No (or dead) battery
        driver_->SetLED(LEDID::START_GN, LEDMode::BLINK_FAST);
      } else if (power_service.GetChargeCurrent() > 0.1f) {  // Battery charging
        driver_->SetLED(LEDID::START_GN, LEDMode::BLINK_SLOW);
      } else {  // Battery charged
        driver_->SetLED(LEDID::START_GN, LEDMode::ON);
      }
    } else {
      // TODO: Handle high level states like "Mowing" or "Area Recording"
      driver_->SetLED(LEDID::START_GN, LEDMode::OFF);
    }
  }
}

void SaboCoverUIController::ThreadFunc() {
  while (true) {
    driver_->ProcessLedStates();
    driver_->LatchLoad();
    DebounceButtons();
    UpdateStates();

    // Sleep for max. 1ms for reliable button debouncing (and future LCD buffer updates (LVGL))
    chThdSleep(TIME_MS2I(1));
  }
}

bool SaboCoverUIController::IsButtonPressed(ButtonID btn) {
  return (btn_stable_states_ & (1 << uint8_t(btn))) == 0;
}

}  // namespace xbot::driver::ui
