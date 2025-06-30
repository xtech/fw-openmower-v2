//
// Created by Apehaenger on 6/23/25.
//

/**
 * @brief Model AGG240160B05?
 *
 * 240*160 pixels, 1-bit monochrome, UC1698u controller
 *
 * PINS:
 * ID0 = 0 => ?
 * ID1 = 0 => 8-bit input data D[0,2,4,6,8,10,12,14]
 * BM[1:0] = 00 => SPI w/ 8-bit token
 * WR[1:0] = NC
 *
 */
#ifndef OPENMOWER_SABO_COVER_UI_DISPLAY_HPP
#define OPENMOWER_SABO_COVER_UI_DISPLAY_HPP

#include <ulog.h>

// #include "ch.h"
// #include "hal.h"
#include "sabo_cover_ui_display_driver_uc1698.hpp"

namespace xbot::driver::ui {

class SaboCoverUIDisplay {
 public:
  static constexpr systime_t backlight_timeout_ = TIME_S2I(120);  // 2 Minutes
  static constexpr systime_t lcd_sleep_timeout_ = TIME_S2I(300);  // 5 Minutes

  static constexpr size_t lcd_width = 240;
  static constexpr size_t lcd_height = 160;

  explicit SaboCoverUIDisplay(LCDCfg lcd_cfg) : lcd_cfg_(lcd_cfg) {
    SaboCoverUIDisplayDriverUC1698::Instance(lcd_cfg, lcd_width, lcd_height);
  }

  bool Init() {
    // Backlight
    if (lcd_cfg_.pins.backlight != PAL_NOLINE) {
      palSetLineMode(lcd_cfg_.pins.backlight, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2);
      palWriteLine(lcd_cfg_.pins.backlight, PAL_LOW);
    }

    // Initialize the LCD driver
    SaboCoverUIDisplayDriverUC1698::Instance().Init();

    return true;
  };

  void Tick() {
    static bool lcd_controller_initialized = false;
    // systime_t now = chVTGetSystemTimeX();
    auto* lcd_driver = SaboCoverUIDisplayDriverUC1698::InstancePtr();

    static systime_t init_time = 0;
    if (!lcd_controller_initialized && lcd_driver) {
      // FIXME: This delay is only for debug measurements
      if (init_time == 0) {
        init_time = chVTGetSystemTimeX();
      }
      // Wait 500ms before handling initialization
      if (chVTTimeElapsedSinceX(init_time) < TIME_MS2I(500)) {
        return;
      }

      lcd_driver->InitController();  // Initialize the LCD controller
      lcd_controller_initialized = true;

      // Now that the LCD got fully initialized, switch backlight on
      WakeUp();

      lcd_driver->SetVBiasPotentiometer(90);  // FIXME: Make configurable */

      auto start = chVTGetSystemTimeX();
      lcd_driver->FillScreen(false);
      auto end = chVTGetSystemTimeX();
      ULOG_INFO("FillScreen took %u us", TIME_I2US(end - start));

      start = chVTGetSystemTimeX();
      lcd_driver->FillScreen(true);
      end = chVTGetSystemTimeX();
      ULOG_INFO("FillScreen took %u us", TIME_I2US(end - start));

      start = chVTGetSystemTimeX();
      lcd_driver->FillScreenFast(false);
      end = chVTGetSystemTimeX();
      ULOG_INFO("FillScreenFast took %u us", TIME_I2US(end - start));
    }

    // Backlight timeout
    if (palReadLine(lcd_cfg_.pins.backlight) == PAL_HIGH &&
        chVTTimeElapsedSinceX(backlight_last_activity_) > backlight_timeout_) {
      palWriteLine(lcd_cfg_.pins.backlight, PAL_LOW);
    }

    // LCD Timeout
    /*if (lcd_awake_ && chVTTimeElapsedSinceX(lcd_last_activity_) > kLcdSleepTimeoutMs) {
        SleepLcd();
    }*/
  };

  void WakeUp() {
    // TODO: Wakeup LCD

    // Backlight on
    palWriteLine(lcd_cfg_.pins.backlight, PAL_HIGH);
    backlight_last_activity_ = chVTGetSystemTimeX();
  }

 protected:
  LCDCfg lcd_cfg_;

  systime_t backlight_last_activity_ = 0;
  systime_t lcd_last_activity_ = 0;
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_DISPLAY_HPP
