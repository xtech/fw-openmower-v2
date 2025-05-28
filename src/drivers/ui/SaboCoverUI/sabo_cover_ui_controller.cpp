//
// Created by Apehaenger on 4/20/25.
//
#include "sabo_cover_ui_controller.hpp"

#include <ulog.h>

#include <services.hpp>

namespace xbot::driver::ui {

static constexpr uint8_t EVT_PACKET_RECEIVED = 1;

void SaboCoverUIController::Configure(const SaboDriverConfig& config) {
  config_ = config;
  configured_ = true;

  // Select the CoverUI driver based on the carrier board version and/or CoverUI Series
  if (carrier_board_info.version_major == 0 && carrier_board_info.version_minor == 1) {
    // HW v0.1 has only CoverUI-Series-II support and no CoverUI-Series detection
    static SaboCoverUIDriverS2HW01 driver_s2hw01(config);
    driver_ = &driver_s2hw01;
  } else {
    // HW v0.2 and later support both CoverUI-Series (I & II) and also has CoverUI-Series detection
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
  PlayPowerOnAnimation();
  chThdSleepMilliseconds(500);
  started_ = true;
}

void SaboCoverUIController::ThreadHelper(void* instance) {
  auto* i = static_cast<SaboCoverUIController*>(instance);
  i->ThreadFunc();
}

void SaboCoverUIController::HandleLEDModes() {
  const systime_t now = chVTGetSystemTime();

  // Slow blink handling
  if (leds_.slow_blink_mask && (now - leds_.last_slow_update >= TIME_MS2I(500))) {
    leds_.last_slow_update = now;
    leds_.slow_blink_state = !leds_.slow_blink_state;
  }

  // Fast blink handling
  if (leds_.fast_blink_mask && (now - leds_.last_fast_update >= TIME_MS2I(100))) {
    leds_.last_fast_update = now;
    leds_.fast_blink_state = !leds_.fast_blink_state;
  }

  // Final LED state calculation
  uint8_t active_leds = leds_.on_mask;
  active_leds |= leds_.slow_blink_state ? leds_.slow_blink_mask : 0;
  active_leds |= leds_.fast_blink_state ? leds_.fast_blink_mask : 0;

  driver_->SetLEDs(active_leds);
  driver_->LatchLoad();
}

void SaboCoverUIController::DebounceButtons() {
  // Debounce all button (at once)
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

  // For identification purposes, Red-Start-LED get handled exclusively with high priority before Green-Start-LED
  if (emergency_service.GetEmergency()) {
    SetLED(LEDID::START_RD, LEDMode::BLINK_FAST);  // Emergency
  } else if (false) {                              // FIXME: Add condition for ROS not alive
    SetLED(LEDID::START_RD, LEDMode::BLINK_SLOW);  // Waiting for ROS
  } else {
    SetLED(LEDID::START_RD, LEDMode::OFF);

    // Green-Start-LED
    if (power_service.GetAdapterVolts() > 26.0f) {    // Docked
      if (power_service.GetBatteryVolts() < 20.0f) {  // No (or dead) battery
        SetLED(LEDID::START_GN, LEDMode::BLINK_FAST);
      } else if (power_service.GetChargeCurrent() > 0.1f) {  // Battery charging
        SetLED(LEDID::START_GN, LEDMode::BLINK_SLOW);
      } else {  // Battery charged
        SetLED(LEDID::START_GN, LEDMode::ON);
      }
    } else {
      // TODO: Handle high level states like "Mowing" or "Area Recording"
      SetLED(LEDID::START_GN, LEDMode::OFF);
    }
  }
}

void SaboCoverUIController::tick() {
  HandleLEDModes();
  DebounceButtons();
  UpdateStates();

  // Debug buttons
  /*static uint16_t last_reported_states_ = 0xFFFF;  // Last debug output
  if (last_reported_states_ != btn_stable_states_) {
    last_reported_states_ = btn_stable_states_;
    ULOG_INFO("DEBUG: Buttons: 0x%04X (Raw: 0x%04X)", (~btn_stable_states_ & 0xFFFF), btn_stable_states_);
  }*/

  // Debug stack size
  /*size_t stack_size = sizeof(wa_);
  size_t unused_stack = (uint8_t*)thread_->ctx.sp - (uint8_t*)&wa_[0];
  size_t used_stack = stack_size - unused_stack;
  ULOG_INFO("DEBUG: Stack: %u/%u used", used_stack, stack_size);*/
}

void SaboCoverUIController::SetLED(LEDID id, LEDMode mode) {
  const uint8_t bit = 1 << uint8_t(id);

  // Clear existing state
  leds_.on_mask &= ~bit;
  leds_.slow_blink_mask &= ~bit;
  leds_.fast_blink_mask &= ~bit;

  // Set new state
  switch (mode) {
    case LEDMode::ON: leds_.on_mask |= bit; break;
    case LEDMode::BLINK_SLOW: leds_.slow_blink_mask |= bit; break;
    case LEDMode::BLINK_FAST: leds_.fast_blink_mask |= bit; break;
    case LEDMode::OFF:
    default: break;
  }
}

void SaboCoverUIController::PlayPowerOnAnimation() {
  leds_.on_mask = 0x1F;  // All on
  chThdSleepMilliseconds(400);
  leds_.on_mask = 0x00;  // All off
  chThdSleepMilliseconds(800);

  // Knight Rider
  for (int i = 0; i <= 5; i++) {
    leds_.on_mask = (1 << i) - 1;
    chThdSleepMilliseconds(80);
  }
  for (int i = 4; i >= 0; i--) {
    leds_.on_mask = (1 << i) - 1;
    chThdSleepMilliseconds(80);
  }
}

void SaboCoverUIController::ThreadFunc() {
  while (true) {
    tick();

    // Wait for event but max. 1ms for reliable button debouncing
    eventmask_t event = chEvtWaitAnyTimeout(EVT_PACKET_RECEIVED, TIME_MS2I(1));
    if (event & EVT_PACKET_RECEIVED) {
      /*chSysLock();
      // Forbid packet reception
      processing = true;
      chSysUnlock();
      if (buffer_fill > 0) {
        ProcessPacket();
      }
      buffer_fill = 0;
      chSysLock();
      // Allow packet reception
      processing = false;
      chSysUnlock();*/
    }
  }
}

bool SaboCoverUIController::IsButtonPressed(ButtonID btn) {
  return (btn_stable_states_ & (1 << uint8_t(btn))) == 0;
}

}  // namespace xbot::driver::ui
