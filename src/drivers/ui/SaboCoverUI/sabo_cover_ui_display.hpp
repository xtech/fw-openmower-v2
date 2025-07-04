//
// Created by Apehaenger on 6/23/25.
//

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
    if (SaboCoverUIDisplayDriverUC1698::Instance().Init()) {
      ULOG_ERROR("SaboCoverUIDisplayDriverUC1698 initialization failed!");
      // We shouldn't fail here for those without a connected display
    }

    return true;
  };

  void Start() {
    auto* lcd_driver = SaboCoverUIDisplayDriverUC1698::InstancePtr();
    if (lcd_driver == nullptr) {
      ULOG_ERROR("Failed getting UC1698 LCD-Driver!");
      return;
    }
    lcd_driver->Start();
  }

  void Tick() {
    // Backlight timeout
    if (palReadLine(lcd_cfg_.pins.backlight) == PAL_HIGH &&
        chVTTimeElapsedSinceX(backlight_last_activity_) > backlight_timeout_) {
      palWriteLine(lcd_cfg_.pins.backlight, PAL_LOW);
    }

    // LCD Timeout
    auto* lcd_driver = SaboCoverUIDisplayDriverUC1698::InstancePtr();
    if (lcd_driver != nullptr) {
      if (lcd_driver->IsDisplayEnabled() && chVTTimeElapsedSinceX(lcd_last_activity_) > lcd_sleep_timeout_) {
        lcd_driver->SetDisplayEnable(false);
      }
    }
  };

  void WakeUp() {
    // Enable Display
    auto* lcd_driver = SaboCoverUIDisplayDriverUC1698::InstancePtr();
    if (lcd_driver != nullptr) {
      if (!lcd_driver->IsDisplayEnabled()) {
        lcd_driver->SetDisplayEnable(true);
      }
    }
    lcd_last_activity_ = chVTGetSystemTimeX();

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
