//
// Created by Apehaenger on 6/23/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_DISPLAY_HPP
#define OPENMOWER_SABO_COVER_UI_DISPLAY_HPP

#include <lvgl.h>
#include <ulog.h>

#include <cstring>

#include "../lvgl/sabo_defs.hpp"
#include "../lvgl/sabo_screen_boot.hpp"
#include "../lvgl/sabo_screen_main.hpp"
#include "ch.h"
#include "sabo_cover_ui_defs.hpp"
#include "sabo_cover_ui_display_driver_uc1698.hpp"

namespace xbot::driver::ui {

using namespace xbot::driver::ui::sabo::display;
using namespace xbot::driver::ui::lvgl;
using namespace xbot::driver::ui::lvgl::sabo;
using DriverUC1698 = SaboCoverUIDisplayDriverUC1698;

using SaboScreenBase = xbot::driver::ui::lvgl::ScreenBase<xbot::driver::ui::lvgl::sabo::ScreenId>;

class SaboCoverUIDisplay {
 public:
  explicit SaboCoverUIDisplay(LCDCfg lcd_cfg) : lcd_cfg_(lcd_cfg) {
  }

  bool Init() {
    // Backlight
    if (lcd_cfg_.pins.backlight != PAL_NOLINE) {
      palSetLineMode(lcd_cfg_.pins.backlight, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2);
      palWriteLine(lcd_cfg_.pins.backlight, PAL_LOW);
    }

    // Initialize and start the LCD driver
    if (!DriverUC1698::Instance(&lcd_cfg_).Init()) {
      ULOG_ERROR("SaboCoverUIDisplayDriverUC1698 initialization failed!");
      return true;  // We shouldn't fail here for those without a connected display
    }

    // Init LVGL, CBs, buffer, display driver, ...
    lv_init();

    // Redirect LVGL log output to ULOG
#if LV_USE_LOG
    lv_log_register_print_cb([](lv_log_level_t level, const char* buf) {
      (void)level;
      ULOG_INFO("[LVGL] %s", buf);
    });
#endif

    lv_tick_set_cb([]() -> uint32_t { return TIME_I2MS(chVTGetSystemTimeX()); });

    // 1/BUFFER_FRACTION screen buffer / 8 pixel per byte, + 2*4 byte palette
    static uint8_t draw_buf1[(LCD_WIDTH * LCD_HEIGHT / BUFFER_FRACTION / 8) + 8];
    lvgl_display_ = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    lv_display_add_event_cb(lvgl_display_, DriverUC1698::LVGLRounderCB, LV_EVENT_INVALIDATE_AREA, NULL);
    lv_display_set_flush_cb(lvgl_display_, DriverUC1698::LVGLFlushCB);
    lv_display_set_flush_wait_cb(lvgl_display_, DriverUC1698::LVGLFlushWaitCB);
    lv_display_set_buffers(lvgl_display_, draw_buf1, NULL, sizeof(draw_buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

    return true;
  };

  void Start() {
    DriverUC1698::Instance().Start();

    ShowBootScreen();
  }

  void SetBootStatus(const etl::string_view& text, int progress) {
    if (screen_boot_) screen_boot_->SetBootStatus(text, progress);
  }

  void ShowBootScreen() {
    if (!screen_boot_) {
      screen_boot_ = new SaboScreenBoot();
      screen_boot_->Create();
    }
    active_screen_ = screen_boot_;
    screen_boot_->Show();
  }

  void ShowMainScreen() {
    if (!screen_main_) {
      screen_main_ = new SaboScreenMain();
      screen_main_->Create();
    }
    active_screen_ = screen_main_;
    screen_main_->Show();
    if (screen_boot_) {
      delete screen_boot_;
      screen_boot_ = nullptr;
    }
  }

  void Tick() {
    // Backlight timeout
    if (palReadLine(lcd_cfg_.pins.backlight) == PAL_HIGH &&
        chVTTimeElapsedSinceX(backlight_last_activity_) > BACKLIGHT_TIMEOUT) {
      palWriteLine(lcd_cfg_.pins.backlight, PAL_LOW);
    }

    if (active_screen_->GetScreenId() == ScreenId::BOOT) {
      // Boot screen is active, switch to main screen after 3 seconds
      // TODO: Change to real service checks or fancy anim
      static systime_t boot_screen_start_time = chVTGetSystemTimeX();
      if (chVTTimeElapsedSinceX(boot_screen_start_time) > TIME_S2I(300)) {
        delete active_screen_;
        active_screen_ = new SaboScreenMain();
        active_screen_->Create();
        active_screen_->Show();
      }
    }

    // LCD & LVGL Timeout
    if (lvgl_display_ && DriverUC1698::Instance().IsDisplayEnabled()) {
      if (chVTTimeElapsedSinceX(lcd_last_activity_) > LCD_SLEEP_TIMEOUT) {
        DriverUC1698::Instance().SetDisplayEnable(false);
      } else {
        // One time Test-Pattern
        static bool test_pattern_drawn = false;
        if (!test_pattern_drawn) {
          test_pattern_drawn = true;
          // create_bouncing_label();
          // create_bouncing_image();
          //  lv_anim_refr_now();
        }
      }
      lv_timer_handler();  // Triggers LVGLFlushCB (if LVGL display content changed)
    }
  }

  void WakeUp() {
    // Enable Display
    if (!DriverUC1698::Instance().IsDisplayEnabled()) {
      DriverUC1698::Instance().SetDisplayEnable(true);
    }
    lcd_last_activity_ = chVTGetSystemTimeX();

    // Backlight on
    palWriteLine(lcd_cfg_.pins.backlight, PAL_HIGH);
    backlight_last_activity_ = chVTGetSystemTimeX();
  }

  SaboScreenBase* GetActiveScreen() const {
    return active_screen_;
  }

  SaboScreenBoot* GetBootScreen() const {
    return screen_boot_;
  }

  SaboScreenMain* GetMainScreen() const {
    return screen_main_;
  }

 private:
  LCDCfg lcd_cfg_;
  lv_display_t* lvgl_display_ = nullptr;  // LVGL display instance

  systime_t backlight_last_activity_ = 0;
  systime_t lcd_last_activity_ = 0;

  SaboScreenBoot* screen_boot_ = nullptr;
  SaboScreenMain* screen_main_ = nullptr;
  SaboScreenBase* active_screen_ = nullptr;
};
}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_DISPLAY_HPP
