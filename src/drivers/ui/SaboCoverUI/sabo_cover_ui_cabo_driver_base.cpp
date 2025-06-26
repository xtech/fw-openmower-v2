//
// Created by Apehaenger on 5/27/25.
//
#include "sabo_cover_ui_cabo_driver_base.hpp"

#include <ulog.h>

namespace xbot::driver::ui {

bool SaboCoverUICaboDriverBase::Init() {
  // Init control pins
  palSetLineMode(sr_pins_.latch_load, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_PULLUP);
  palWriteLine(sr_pins_.latch_load, PAL_LOW);  // HC595 RCLK/PL (parallel load)

  if (sr_pins_.oe != PAL_NOLINE) {
    palSetLineMode(sr_pins_.oe, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_FLOATING);
    palWriteLine(sr_pins_.oe, PAL_HIGH);  // /OE (output enable = no)
  }

  if (sr_pins_.btn_cs != PAL_NOLINE) {
    palSetLineMode(sr_pins_.btn_cs, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_PULLUP);
    palWriteLine(sr_pins_.btn_cs, PAL_HIGH);  // /CS (chip select = no)
  }

  if (sr_pins_.inp_cs != PAL_NOLINE) {
    palSetLineMode(sr_pins_.inp_cs, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_PULLUP);
    palWriteLine(sr_pins_.inp_cs, PAL_HIGH);  // /CS (chip select = no)
  }

  return true;
}

void SaboCoverUICaboDriverBase::SetLED(LEDID id, LEDMode mode) {
  const uint8_t bit = MapLEDIDToMask(id);

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

void SaboCoverUICaboDriverBase::PowerOnAnimation() {
  // All on
  for (int i = 0; i < 5; ++i) SetLED(static_cast<LEDID>(i), LEDMode::ON);
  ProcessLedStates();
  LatchLoad();
  chThdSleepMilliseconds(500);

  // All off
  for (int i = 0; i < 5; ++i) SetLED(static_cast<LEDID>(i), LEDMode::OFF);
  ProcessLedStates();
  LatchLoad();
  chThdSleepMilliseconds(800);

  // Knight Rider
  for (int i = 0; i <= 5; i++) {
    for (int j = 0; j < 5; ++j) SetLED(static_cast<LEDID>(j), (j < i) ? LEDMode::ON : LEDMode::OFF);
    ProcessLedStates();
    LatchLoad();
    chThdSleepMilliseconds(100);
  }
  for (int i = 4; i >= 0; i--) {
    for (int j = 0; j < 5; ++j) SetLED(static_cast<LEDID>(j), (j < i) ? LEDMode::ON : LEDMode::OFF);
    ProcessLedStates();
    LatchLoad();
    chThdSleepMilliseconds(100);
  }
}

void SaboCoverUICaboDriverBase::ProcessLedStates() {
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
  current_led_mask_ = leds_.on_mask;
  current_led_mask_ |= leds_.slow_blink_state ? leds_.slow_blink_mask : 0;
  current_led_mask_ |= leds_.fast_blink_state ? leds_.fast_blink_mask : 0;
}

void SaboCoverUICaboDriverBase::DebounceRawButtons(const uint16_t raw_buttons) {
  static uint16_t btn_last_raw_mask_ = 0xFFFF;  // Last raw button state
  static size_t btn_debounce_counter_ = 0;      // If >= DEBOUNCE_TICKS, the button state is stable/debounced

  const uint16_t changed_bits = btn_last_raw_mask_ ^ raw_buttons;  // XOR to find changed bits
  if (changed_bits == 0) {
    if (btn_debounce_counter_ < DEBOUNCE_TICKS) btn_debounce_counter_++;
  } else {
    btn_debounce_counter_ = 0;
  }
  btn_stable_raw_mask_ = (btn_debounce_counter_ >= DEBOUNCE_TICKS) ? raw_buttons : btn_stable_raw_mask_;
  btn_last_raw_mask_ = raw_buttons;
}

bool SaboCoverUICaboDriverBase::IsButtonPressed(const ButtonID btn) const {
  if (!series_) return false;
  auto btn_mask = series_->MapButtonIDToMask(btn);
  if (btn_mask == 0) return false;                                       // Unknown button ID = "not pressed"
  return (btn_stable_raw_mask_ & series_->MapButtonIDToMask(btn)) == 0;  // low-active
}

bool SaboCoverUICaboDriverBase::IsReady() const {
  return state_ == DriverState::READY;
}

void SaboCoverUICaboDriverBase::Tick() {
  ProcessLedStates();

  switch (state_) {
    case DriverState::WAITING_FOR_CUI:
      // Wait for CoverUI to be connected
      series_ = GetSeriesDriver();
      if (series_) {
        state_ = DriverState::BOOT_ANIMATION;
        chThdSleepMilliseconds(200);
        PowerOnAnimation();
        chThdSleepMilliseconds(500);
        state_ = DriverState::READY;
      }
      break;
    case DriverState::BOOT_ANIMATION:
      // Do nothing till boot animation finished
      break;

    case DriverState::READY:
      LatchLoad();  // Also has to call DebounceRawButtons()
      break;
  }

  // Debug log for stable button state
  /*static systime_t last_log_time = 0;
  systime_t now = chVTGetSystemTime();
  if (now - last_log_time >= TIME_S2I(1)) {
    last_log_time = now;
    ULOG_INFO("Sabo CoverUI [btn_stable_raw_mask_: 0x%04X]", btn_stable_raw_mask_);
  }*/
}

}  // namespace xbot::driver::ui
