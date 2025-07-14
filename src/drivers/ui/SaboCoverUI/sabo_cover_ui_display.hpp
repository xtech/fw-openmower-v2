//
// Created by Apehaenger on 6/23/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_DISPLAY_HPP
#define OPENMOWER_SABO_COVER_UI_DISPLAY_HPP

#include <ulog.h>

#include <cstring>

#include "ch.h"
#include "lvgl.h"
#include "sabo_cover_ui_defs.hpp"
#include "sabo_cover_ui_display_driver_uc1698.hpp"

namespace xbot::driver::ui {

using namespace sabo::display;

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
    if (!SaboCoverUIDisplayDriverUC1698::Instance(&lcd_cfg_).Init()) {
      ULOG_ERROR("SaboCoverUIDisplayDriverUC1698 initialization failed!");
      return true;  // We shouldn't fail here for those without a connected display
    }

    // Init LVGL, CBs, buffer, display driver, ...
    lv_init();
    lv_tick_set_cb(GetMillis);

    // 1/10 screen buffer / 8 pixel per byte, + 2*4 byte palette
    static uint8_t draw_buf1[(LCD_WIDTH * LCD_HEIGHT / BUFFER_FRACTION / 8) + 8];
    lvgl_display_ = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    lv_display_add_event_cb(lvgl_display_, SaboCoverUIDisplayDriverUC1698::LVGLRounderCB, LV_EVENT_INVALIDATE_AREA,
                            NULL);
    lv_display_set_flush_cb(lvgl_display_, SaboCoverUIDisplayDriverUC1698::LVGLFlushCB);
    lv_display_set_flush_wait_cb(lvgl_display_, SaboCoverUIDisplayDriverUC1698::LVGLFlushWaitCB);
    lv_display_set_buffers(lvgl_display_, draw_buf1, NULL, sizeof(draw_buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

    return true;
  };

  void Start() {
    SaboCoverUIDisplayDriverUC1698::Instance().Start();
  }

  void Tick() {
    // Backlight timeout
    if (palReadLine(lcd_cfg_.pins.backlight) == PAL_HIGH &&
        chVTTimeElapsedSinceX(backlight_last_activity_) > BACKLIGHT_TIMEOUT) {
      palWriteLine(lcd_cfg_.pins.backlight, PAL_LOW);
    }

    // LCD & LVGL Timeout
    if (lvgl_display_ && SaboCoverUIDisplayDriverUC1698::Instance().IsDisplayEnabled()) {
      if (chVTTimeElapsedSinceX(lcd_last_activity_) > LCD_SLEEP_TIMEOUT) {
        SaboCoverUIDisplayDriverUC1698::Instance().SetDisplayEnable(false);
      } else {
        // One time Test-Pattern
        static bool test_pattern_drawn = false;
        if (!test_pattern_drawn) {
          test_pattern_drawn = true;
          create_bouncing_label();
        }
      }
      lv_timer_handler();  // Triggers LVGLFlushCB (if LVGL display content changed)
    }
  }

  void WakeUp() {
    // Enable Display
    if (!SaboCoverUIDisplayDriverUC1698::Instance().IsDisplayEnabled())
      SaboCoverUIDisplayDriverUC1698::Instance().SetDisplayEnable(true);
    lcd_last_activity_ = chVTGetSystemTimeX();

    // Backlight on
    palWriteLine(lcd_cfg_.pins.backlight, PAL_HIGH);
    backlight_last_activity_ = chVTGetSystemTimeX();
  }

  static uint32_t GetMillis(void) {
    return TIME_I2MS(chVTGetSystemTimeX());
  }

 private:
  LCDCfg lcd_cfg_;
  lv_display_t* lvgl_display_ = nullptr;  // LVGL display instance

  systime_t backlight_last_activity_ = 0;
  systime_t lcd_last_activity_ = 0;

  // ----- As long as we've no GUI, let's play Rovo's bounce anim -----
  typedef struct {
    lv_obj_t* obj;
    int dx;
    int dy;
  } label_anim_data_t;

  static void bounce_anim_cb(lv_timer_t* timer) {
    label_anim_data_t* data = (label_anim_data_t*)lv_timer_get_user_data(timer);
    lv_obj_t* obj = data->obj;

    // Get current position
    lv_coord_t x = lv_obj_get_x(obj);
    lv_coord_t y = lv_obj_get_y(obj);

    // Get object size and screen size
    lv_coord_t w = lv_obj_get_width(obj);
    lv_coord_t h = lv_obj_get_height(obj);
    lv_coord_t sw = lv_obj_get_width(lv_screen_active());
    lv_coord_t sh = lv_obj_get_height(lv_screen_active());

    // Update position
    x += data->dx;
    y += data->dy;

    // Bounce from edges
    if (x <= 0 || (x + w) >= sw) {
      data->dx = -data->dx;
      x += data->dx;
    }

    if (y <= 0 || (y + h) >= sh) {
      data->dy = -data->dy;
      y += data->dy;
    }

    // Set new position
    lv_obj_set_pos(obj, x, y);
  }

  void create_bouncing_label() {
    lv_obj_t* label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "OpenMower");
    lv_obj_set_style_text_color(label, lv_color_black(), LV_PART_MAIN);

    // lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    // Create animation data
    static label_anim_data_t anim_data = {.obj = label, .dx = 1, .dy = 1};

    // Create timer to update position
    lv_timer_create(bounce_anim_cb, 100, &anim_data);
  }
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_DISPLAY_HPP
