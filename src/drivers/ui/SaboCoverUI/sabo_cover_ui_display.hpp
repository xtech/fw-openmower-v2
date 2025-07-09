//
// Created by Apehaenger on 6/23/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_DISPLAY_HPP
#define OPENMOWER_SABO_COVER_UI_DISPLAY_HPP

#include <ulog.h>

#include "ch.h"
#include "sabo_cover_ui_display_driver_uc1698.hpp"

// LVGL includes
#include <cstring>

#include "lvgl.h"

#define SABO_LCD_WIDTH 240  // ATTENTION: LVGL I1 mode requires a multiple of 8 width
#define SABO_LCD_HEIGHT 160

namespace xbot::driver::ui {

class SaboCoverUIDisplay {
 public:
  static constexpr systime_t backlight_timeout_ = TIME_S2I(120);  // 2 Minutes
  static constexpr systime_t lcd_sleep_timeout_ = TIME_S2I(300);  // 5 Minutes

  explicit SaboCoverUIDisplay(LCDCfg lcd_cfg) : lcd_cfg_(lcd_cfg) {
    SaboCoverUIDisplayDriverUC1698::Instance(lcd_cfg);
  }

  bool Init() {
    // Backlight
    if (lcd_cfg_.pins.backlight != PAL_NOLINE) {
      palSetLineMode(lcd_cfg_.pins.backlight, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2);
      palWriteLine(lcd_cfg_.pins.backlight, PAL_LOW);
    }

    // Initialize and start the LCD driver
    if (!SaboCoverUIDisplayDriverUC1698::Instance().Init()) {
      ULOG_ERROR("SaboCoverUIDisplayDriverUC1698 initialization failed!");
      return true;  // We shouldn't fail here for those without a connected display
    }

    // Init LVGL, CBs, buffer, display driver, ...
    lv_init();
    lv_tick_set_cb(GetMillis);

    // 1/10 screen buffer / 8 pixel per byte, + 2*4 byte palette
    static uint8_t draw_buf1[(SABO_LCD_WIDTH * SABO_LCD_HEIGHT / 10 / 8) + 8];
    lvgl_display_ = lv_display_create(SABO_LCD_WIDTH, SABO_LCD_HEIGHT);
    lv_display_add_event_cb(lvgl_display_, SaboCoverUIDisplayDriverUC1698::LVGLRounderCallback,
                            LV_EVENT_INVALIDATE_AREA, NULL);
    lv_display_set_flush_cb(lvgl_display_, SaboCoverUIDisplayDriverUC1698::LVGLFlushCallback);
    lv_display_set_buffers(lvgl_display_, draw_buf1, NULL, sizeof(draw_buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

    return true;
  };

  void Tick() {
    // Backlight timeout
    if (palReadLine(lcd_cfg_.pins.backlight) == PAL_HIGH &&
        chVTTimeElapsedSinceX(backlight_last_activity_) > backlight_timeout_) {
      palWriteLine(lcd_cfg_.pins.backlight, PAL_LOW);
    }

    // LCD & LVGL Timeout
    auto* lcd_driver = SaboCoverUIDisplayDriverUC1698::InstancePtr();
    if (lcd_driver != nullptr) {
      if (lcd_driver->IsDisplayEnabled()) {
        if (chVTTimeElapsedSinceX(lcd_last_activity_) > lcd_sleep_timeout_) {
          lcd_driver->SetDisplayEnable(false);
        } else {
          // One time Test-Pattern
          static bool test_pattern_drawn = false;
          if (!test_pattern_drawn) {
            LCDTestPattern1();
            lv_timer_handler();  // Handle LVGL timer
            LCDTestPattern2();
            lv_timer_handler();  // Handle LVGL timer
            HelloWorld();
            test_pattern_drawn = true;
          }
          lv_timer_handler();  // Handle LVGL timer
        }
      }
    }
  }

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

  static uint32_t GetMillis(void) {
    return TIME_I2MS(chVTGetSystemTimeX());
  }

 protected:
  LCDCfg lcd_cfg_;

  systime_t backlight_last_activity_ = 0;
  systime_t lcd_last_activity_ = 0;

  lv_display_t* lvgl_display_ = nullptr;  // LVGL display instance

  void LCDTestPattern1() {
    /*Change the active screen's background color*/
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_white(), LV_PART_MAIN);

    // Outer  Rectangle
    lv_obj_t* outer = lv_obj_create(lv_screen_active());
    lv_obj_set_style_border_width(outer, 1, 0);
    lv_obj_set_style_border_color(outer, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(outer, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(outer, LV_ALIGN_TOP_LEFT, 0, 0);

    // lv_obj_set_size(outer, SABO_LCD_WIDTH, SABO_LCD_HEIGHT);
    lv_obj_set_size(outer, 10, 5);
    lv_obj_set_pos(outer, 0, 0);

    // Test rectangles with different gray shades
    /*uint8_t grays[4] = {192, 128, 64, 26};  // 75%, 50%, 25%, 10% Gray (0=schwarz, 255=weiß)

    for (int i = 0; i < 4; ++i) {
      lv_obj_t* rect = lv_obj_create(lv_screen_active());
      lv_obj_set_style_border_width(rect, 1, 0);
      lv_obj_set_style_border_color(rect, lv_color_black(), 0);
      lv_obj_set_style_bg_color(rect, lv_color_make(grays[i], grays[i], grays[i]), 0);
      lv_obj_set_size(rect, (SABO_LCD_WIDTH - (i * 2) - 2), (SABO_LCD_HEIGHT - (i * 2) - 2));
      lv_obj_set_pos(rect, (SABO_LCD_WIDTH + (i * 2) + 1), (SABO_LCD_HEIGHT + (i * 2) + 1));
    }*/
  }

  void LCDTestPattern2() {
    /*Change the active screen's background color*/
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_white(), LV_PART_MAIN);

    // Outer  Rectangle
    lv_obj_t* outer = lv_obj_create(lv_screen_active());
    lv_obj_set_style_border_width(outer, 1, 0);
    lv_obj_set_style_border_color(outer, lv_color_black(), 0);

    // lv_obj_set_style_bg_opa(outer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(outer, LV_OPA_COVER, 0);

    // lv_obj_set_size(outer, SABO_LCD_WIDTH, SABO_LCD_HEIGHT);
    lv_obj_set_size(outer, 10, 5);
    lv_obj_set_pos(outer, 10, 10);

    // Test rectangles with different gray shades
    /*uint8_t grays[4] = {192, 128, 64, 26};  // 75%, 50%, 25%, 10% Gray (0=schwarz, 255=weiß)

    for (int i = 0; i < 4; ++i) {
      lv_obj_t* rect = lv_obj_create(lv_screen_active());
      lv_obj_set_style_border_width(rect, 1, 0);
      lv_obj_set_style_border_color(rect, lv_color_black(), 0);
      lv_obj_set_style_bg_color(rect, lv_color_make(grays[i], grays[i], grays[i]), 0);
      lv_obj_set_size(rect, (SABO_LCD_WIDTH - (i * 2) - 2), (SABO_LCD_HEIGHT - (i * 2) - 2));
      lv_obj_set_pos(rect, (SABO_LCD_WIDTH + (i * 2) + 1), (SABO_LCD_HEIGHT + (i * 2) + 1));
    }*/
  }

  void HelloWorld(void) {
    /*Change the active screen's background color*/
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_white(), LV_PART_MAIN);

    /*Create a white label, set its text and align it to the center*/
    lv_obj_t* label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello world!");
    lv_obj_set_style_text_color(lv_screen_active(), lv_color_black(), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
  }
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_DISPLAY_HPP
