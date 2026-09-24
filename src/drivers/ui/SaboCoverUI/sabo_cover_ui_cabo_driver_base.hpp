//
// Created by Apehaenger on 5/26/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_CABO_DRIVER_BASE_HPP
#define OPENMOWER_SABO_COVER_UI_CABO_DRIVER_BASE_HPP

#include "ch.h"
#include "hal.h"
#include "robots/include/sabo_common.hpp"
#include "sabo_cover_ui_series_interface.hpp"

namespace xbot::driver::ui {

using namespace xbot::driver::sabo;

class SaboCoverUICaboDriverBase {
 public:
  explicit SaboCoverUICaboDriverBase(const xbot::driver::sabo::config::CoverUi* cover_ui_cfg)
      : cover_ui_cfg_(cover_ui_cfg){};

  // 6 * 10ms(tick) / 2(alternating button rows) = 30ms debounce time
  // Series-I has no alternating button rows, so it will debounce in 60ms (who cares)
  static constexpr uint8_t DEBOUNCE_TICKS = 5;

  virtual bool Init();              // Init GPIOs, SPI and assign series_ driver
  virtual void LatchLoad() = 0;     // Latch data (LEDs, Button-rows, signals) and load inputs (buttons, signals, ...)
  virtual void PowerOnAnimation();  // KIT like anim
  virtual void Tick();              // Call this function every 1ms to update LEDs, read and debounce buttons, ...

  bool IsButtonPressed(ButtonId btn) const;  // Check if a specific button is pressed
  bool IsAnyButtonPressed() const;           // Check if any button is pressed

  /**
   * @brief Get all button states as a standardized bitmask
   * @return Bitmask where bit N is set (1) if button N is pressed, 0 if not pressed
   * Handles conversion from Series-I/II specific button mapping to standardized format
   * For button IDs see xbot::driver::sabo::types::ButtonID enum
   */
  uint16_t GetButtonsMask() const;

  /**
   * @brief Get the Series Type object
   *
   * @return SeriesType
   */
  SeriesType GetSeriesType() const {
    return series_ ? series_->GetType() : SeriesType::Unknown;
  }

  bool IsReady() const;  // True if CoverUI detected, boot anim played and ready to serve requests

  void SetLed(LedId id, LedMode mode);  // Set state of a single LED

  // Debounce all raw buttons at once in one quick XOR operation
  // This Method needs to be called by driver implementation once it read the buttons
  void DebounceRawButtons(uint16_t raw_buttons);

 protected:
  const xbot::driver::sabo::config::CoverUi* cover_ui_cfg_;
  SPIConfig spi_config_;
  SaboCoverUISeriesInterface* series_ = nullptr;  // Series-I/II specific driver

  struct LEDState {
    uint16_t on_mask = 0;
    uint16_t slow_blink_mask = 0;
    uint16_t fast_blink_mask = 0;
    systime_t last_slow_update = 0;
    systime_t last_fast_update = 0;
    bool slow_blink_state = false;
    bool fast_blink_state = false;
  };
  LEDState leds_;

  enum class DriverState {
    WAITING_FOR_CUI,  // Waiting for CoverUI to be connected
    BOOT_ANIMATION,   // Bootup- animation started/running
    READY             // Driver is ready to server requests
  };
  DriverState state_ = DriverState::WAITING_FOR_CUI;

  uint16_t current_led_mask_ = 0;  // Series specific current LEDs, with applied LED modes, high-active

  virtual SaboCoverUISeriesInterface* GetSeriesDriver() = 0;  // Get the CoverUI Series driver, if connected
  virtual uint16_t MapLedIdToMask(LedId id) const = 0;

  void ProcessLedStates();  // Process the different LED modes (on, blink, ...)

  /**
   * @brief Busy-wait delay in microseconds, based on the DWT cycle counter.
   *
   * Needed for the CoverUI Series-II control signals (HEF4794BT STR, HC165 SH/LD).
   * Their 5V HIGH level is built by pull-ups only (open-drain level shifter, e.g. SN74LVC07A
   * as of carrierboard v0.6), so the rising edge is an RC edge in the range of some 10 to
   * 100ns. A strobe created by back-to-back palWriteLine() calls lasts only a few CPU cycles
   * (~15ns @ 550MHz in -Os builds) and would therefore never reach the input threshold of the
   * 5V devices. This busy-wait makes the strobe/setup times deterministic and independent of
   * the build's optimization level, the instruction cache and the kernel tick frequency.
   *
   * @note The DWT cycle counter counts CPU/core cycles (STM32_SYS_CK), not AHB cycles
   *       (STM32_HCLK, which is SYSCLK/2 as of carrierboard v0.6). A wrong clock assumption
   *       by a factor of two is harmless here, the delay only has to be >= 5 * R * C.
   * @note The DWT cycle counter gets enabled in Init().
   * @param[in] us        microseconds to wait for
   */
  static void DelayMicroseconds(uint32_t us) {
#if defined(STM32_SYS_CK)
    const uint32_t cycles_per_us = STM32_SYS_CK / 1000000U;  // STM32H7: CPU clock, see note
#else
    const uint32_t cycles_per_us = STM32_HCLK / 1000000U;
#endif
    const uint32_t delay_cycles = us * cycles_per_us;
    const uint32_t start_cycles = DWT->CYCCNT;
    while ((DWT->CYCCNT - start_cycles) < delay_cycles) {
    }
  }

  uint16_t btn_stable_raw_mask_ = 0xFFFF;  // Stable (debounced) button state
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_CABO_DRIVER_BASE_HPP
