//
// Created by Apehaenger on 4/20/25.
//
#include "sabo_ui_controller.hpp"

#include <ulog.h>

#include <services.hpp>

static constexpr uint8_t EVT_PACKET_RECEIVED = 1;

void SaboUIController::Start() {
  if (thread_ != nullptr) {
    ULOG_ERROR("Started Sabo UI Controller twice!");
    return;
  }

  if (driver_ == nullptr) {
    ULOG_ERROR("Sabo UI Driver not set!");
    return;
  }
  if (!driver_->init()) {
    ULOG_ERROR("Failed to initialize Sabo UI Driver!");
    return;
  }

  thread_ = chThdCreateStatic(&wa_, sizeof(wa_), NORMALPRIO, ThreadHelper, this);
#ifdef USE_SEGGER_SYSTEMVIEW
  processing_thread_->name = "SaboUIController";
#endif

  // Now that driver is initialized and thread got started, we can enable output
  driver_->enableOutput();
  chThdSleepMilliseconds(100);
  PlayPowerOnAnimation();
  chThdSleepMilliseconds(500);
  started_ = true;
}

void SaboUIController::ThreadHelper(void* instance) {
  auto* i = static_cast<SaboUIController*>(instance);
  i->ThreadFunc();
}

void SaboUIController::HandleLEDModes() {
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

  driver_->setLEDs(active_leds);
  driver_->latchLoad();
}

void SaboUIController::DebounceButtons() {
  // Debounce all button (at once)
  const uint16_t raw = driver_->getRawButtonStates();
  const uint16_t changed_bits = btn_last_raw_ ^ raw;  // XOR to find changed bits
  if (changed_bits == 0) {
    if (btn_debounce_counter_ < DEBOUNCE_TICKS) btn_debounce_counter_++;
  } else {
    btn_debounce_counter_ = 0;
  }
  btn_stable_states_ = (btn_debounce_counter_ >= DEBOUNCE_TICKS) ? raw : btn_stable_states_;
  btn_last_raw_ = raw;
}

void SaboUIController::UpdateStates() {
  if (!started_) return;

  // For identification purposes, Red-Start-LED get handled with high priority
  if (emergency_service.GetEmergency()) {
    SetLED(LEDID::START_RD, LEDMode::BLINK_FAST);  // Emergency
  } else if (power_service.GetAdapterVoltage() > 26.0f && power_service.GetChargeCurrent() > 0.2f) {
    SetLED(LEDID::START_RD, LEDMode::BLINK_SLOW);  // Docked and charging
  } else {
    SetLED(LEDID::START_RD, LEDMode::OFF);

    // Green-Start-LED
    if (power_service.GetAdapterVoltage() > 26.0f) {    // Docked
      if (power_service.GetBatteryVoltage() < 20.0f) {  // No (or dead) battery
        SetLED(LEDID::START_GN, LEDMode::BLINK_FAST);
      } else {  // Battery charged
        SetLED(LEDID::START_GN, LEDMode::BLINK_SLOW);
      }
    } else {
      // TODO: Handle high level states like "Mowing" or "Area Recording"
      SetLED(LEDID::START_GN, LEDMode::OFF);
    }
  }
}

void SaboUIController::tick() {
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

void SaboUIController::SetLED(LEDID id, LEDMode mode) {
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

void SaboUIController::PlayPowerOnAnimation() {
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

void SaboUIController::ThreadFunc() {
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

bool SaboUIController::IsButtonPressed(ButtonID btn) {
  return (btn_stable_states_ & (1 << uint8_t(btn))) == 0;
}
