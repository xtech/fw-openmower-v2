//
// Created by Apehaenger on 5/27/25.
//
#include "sabo_cover_ui_cabo_driver_base.hpp"

#include <ulog.h>

namespace xbot::driver::ui {

bool SaboCoverUICaboDriverBase::Init() {
  // Init SPI pins
  if (cover_ui_cfg_->spi.instance != nullptr) {
    palSetLineMode(cover_ui_cfg_->spi.pins.sck, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_MID2);
    palSetLineMode(cover_ui_cfg_->spi.pins.miso,
                   PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_PULLUP);
    palSetLineMode(cover_ui_cfg_->spi.pins.mosi, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_MID2);
    if (cover_ui_cfg_->spi.pins.cs != PAL_NOLINE) {
      palSetLineMode(cover_ui_cfg_->spi.pins.cs, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2);
      palWriteLine(cover_ui_cfg_->spi.pins.cs, PAL_HIGH);
    }
  }

  // Init Cabo's shift register control pins
  palSetLineMode(cover_ui_cfg_->pins.latch_load, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2);
  palWriteLine(cover_ui_cfg_->pins.latch_load, PAL_LOW);  // HC595 RCLK/PL (parallel load)
  if (cover_ui_cfg_->pins.oe != PAL_NOLINE) {
    palSetLineMode(cover_ui_cfg_->pins.oe, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2);
    palWriteLine(cover_ui_cfg_->pins.oe, PAL_HIGH);  // /OE (output enable = no)
  }
  if (cover_ui_cfg_->pins.btn_cs != PAL_NOLINE) {
    palSetLineMode(cover_ui_cfg_->pins.btn_cs, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2);
    palWriteLine(cover_ui_cfg_->pins.btn_cs, PAL_HIGH);  // /CS (chip select = no)
  }
  if (cover_ui_cfg_->pins.inp_cs != PAL_NOLINE) {
    palSetLineMode(cover_ui_cfg_->pins.inp_cs, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2);
    palWriteLine(cover_ui_cfg_->pins.inp_cs, PAL_HIGH);  // /CS (chip select = no)
  }

  return true;
}

void SaboCoverUICaboDriverBase::SetLed(LedId id, LedMode mode) {
  const uint8_t bit = MapLedIdToMask(id);

  // Clear existing state
  leds_.on_mask &= ~bit;
  leds_.slow_blink_mask &= ~bit;
  leds_.fast_blink_mask &= ~bit;

  // Set new state
  switch (mode) {
    case LedMode::ON: leds_.on_mask |= bit; break;
    case LedMode::BLINK_SLOW: leds_.slow_blink_mask |= bit; break;
    case LedMode::BLINK_FAST: leds_.fast_blink_mask |= bit; break;
    case LedMode::OFF:
    default: break;
  }
}

void SaboCoverUICaboDriverBase::PowerOnAnimation() {
  // All on
  for (int i = 0; i < 5; ++i) SetLed(static_cast<LedId>(i), LedMode::ON);
  ProcessLedStates();
  LatchLoad();
  chThdSleepMilliseconds(500);

  // All off
  for (int i = 0; i < 5; ++i) SetLed(static_cast<LedId>(i), LedMode::OFF);
  ProcessLedStates();
  LatchLoad();
  chThdSleepMilliseconds(800);

  // Knight Rider
  for (int i = 0; i <= 5; i++) {
    for (int j = 0; j < 5; ++j) SetLed(static_cast<LedId>(j), (j < i) ? LedMode::ON : LedMode::OFF);
    ProcessLedStates();
    LatchLoad();
    chThdSleepMilliseconds(100);
  }
  for (int i = 4; i >= 0; i--) {
    for (int j = 0; j < 5; ++j) SetLed(static_cast<LedId>(j), (j < i) ? LedMode::ON : LedMode::OFF);
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

void SaboCoverUICaboDriverBase::DebounceRawButtons(uint16_t raw_buttons) {
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

bool SaboCoverUICaboDriverBase::IsButtonPressed(ButtonId btn) const {
  if (!series_) return false;
  auto btn_mask = series_->MapButtonIDToMask(btn);
  if (btn_mask == 0) return false;                                       // Unknown button ID = "not pressed"
  return (btn_stable_raw_mask_ & series_->MapButtonIDToMask(btn)) == 0;  // low-active
}

bool SaboCoverUICaboDriverBase::IsAnyButtonPressed() const {
  if (!series_) return false;
  return (btn_stable_raw_mask_ & series_->AllButtonsMask()) != series_->AllButtonsMask();  // low-active
}

uint16_t SaboCoverUICaboDriverBase::GetButtonsMask() const {
  if (!series_) return 0;  // No buttons pressed if driver not ready

  uint16_t standardized_mask = 0;
  // Convert each button from series-specific format to standardized format
  for (const auto& button_id : defs::ALL_BUTTONS) {
    auto series_bit = series_->MapButtonIDToMask(button_id);
    if (series_bit != 0 && (btn_stable_raw_mask_ & series_bit) == 0) {  // low-active: 0 = pressed
      standardized_mask |= (1 << static_cast<uint16_t>(button_id));
    }
  }
  return standardized_mask;  // 1 = pressed, 0 = not pressed
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
}

}  // namespace xbot::driver::ui
