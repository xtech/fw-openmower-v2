//
// Created by Apehaenger on 5/27/25.
//
#include "sabo_cover_ui_driver_base.hpp"

#include <ulog.h>

namespace xbot::driver::ui {

bool SaboCoverUIDriverBase::Init() {
  // Init control pins
  palSetLineMode(config_.control_pins.latch_load,
                 PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_FLOATING);
  palSetLineMode(config_.control_pins.oe, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_FLOATING);
  if (config_.control_pins.btn_cs != PAL_NOLINE) {
    palSetLineMode(config_.control_pins.btn_cs,
                   PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_FLOATING);
  }
  if (config_.control_pins.inp_cs != PAL_NOLINE) {
    palSetLineMode(config_.control_pins.inp_cs,
                   PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_FLOATING);
  }

  // Init SPI pins
  palSetLineMode(config_.spi_pins.sck, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_FLOATING);
  palSetLineMode(config_.spi_pins.miso, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_FLOATING);
  palSetLineMode(config_.spi_pins.mosi, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_FLOATING);

  // Configure SPI
  spi_cfg_ = {
      .circular = false,
      .slave = false,
      .data_cb = nullptr,
      .error_cb = nullptr,
      .ssline = 0,                                                     // Master mode
      .cfg1 = SPI_CFG1_MBR_0 | SPI_CFG1_MBR_1 |                        // Baudrate = FPCLK/16 (~10 MHz @ 160 MHz SPI-C)
              SPI_CFG1_DSIZE_2 | SPI_CFG1_DSIZE_1 | SPI_CFG1_DSIZE_0,  // 8-Bit (DS = 0b111)
      .cfg2 = SPI_CFG2_MASTER                                          // Master mode
  };

  // Start SPI
  if (spiStart(config_.spi_instance, &spi_cfg_) != MSG_OK) {
    ULOG_ERROR("CoverUI SPI init failed");
    return false;
  }

  return true;
}

void SaboCoverUIDriverBase::SetLED(LEDID id, LEDMode mode) {
  const uint8_t bit = MapLEDIDToBit(id);

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

void SaboCoverUIDriverBase::ProcessLedStates() {
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

void SaboCoverUIDriverBase::DebounceButtons() {
  // Debounce all buttons (at once)
  const uint16_t changed_bits = btn_last_raw_mask_ ^ btn_cur_raw_mask_;  // XOR to find changed bits
  if (changed_bits == 0) {
    if (btn_debounce_counter_ < DEBOUNCE_TICKS) btn_debounce_counter_++;
  } else {
    btn_debounce_counter_ = 0;
  }
  btn_stable_raw_mask_ = (btn_debounce_counter_ >= DEBOUNCE_TICKS) ? btn_cur_raw_mask_ : btn_stable_raw_mask_;
  btn_last_raw_mask_ = btn_cur_raw_mask_;
}

void SaboCoverUIDriverBase::Tick() {
  ProcessLedStates();
  LatchLoad();
  DebounceButtons();
}

}  // namespace xbot::driver::ui
