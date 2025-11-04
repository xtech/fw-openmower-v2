/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sabo_screen_config.hpp
 * @brief Configuration screen for LCD settings (Contrast, Temp. Compensation, Auto-Sleep)
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-11-04
 */

#ifndef LVGL_SABO_SCREEN_CONFIG_HPP_
#define LVGL_SABO_SCREEN_CONFIG_HPP_

#include <lvgl.h>

#include "../../SaboCoverUI/sabo_cover_ui_defs.hpp"
#include "../screen_base.hpp"
#include "sabo_defs.hpp"

extern "C" {
LV_FONT_DECLARE(orbitron_12);
LV_FONT_DECLARE(orbitron_16b);
}

namespace xbot::driver::ui::lvgl::sabo {

using namespace xbot::driver::ui::sabo::display;  // For getting LCD_WIDTH, LCD_HEIGHT

class SaboScreenConfig : public ScreenBase<ScreenId, xbot::driver::ui::sabo::ButtonID> {
 public:
  SaboScreenConfig() : ScreenBase<ScreenId, xbot::driver::ui::sabo::ButtonID>(sabo::ScreenId::CONFIG) {
  }

  struct Config {
    uint8_t contrast = 100;          // 0-255 (default 100)
    uint8_t temp_compensation = 2;   // 0=Off, 1=Low, 2=Medium, 3=High (default 2)
    uint8_t auto_sleep_minutes = 5;  // 1-20 minutes (default 5)
  };

  void Create(lv_color_t bg_color = lv_color_white()) override {
    ScreenBase::Create(bg_color);

    // Title
    lv_obj_t* title = lv_label_create(screen_);
    lv_label_set_text(title, "LCD Configuration");
    lv_obj_set_style_text_font(title, &orbitron_16b, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    // Separator line
    lv_obj_t* separator = lv_obj_create(screen_);
    lv_obj_set_size(separator, LCD_WIDTH, 1);
    lv_obj_set_pos(separator, 0, 20);
    lv_obj_set_style_bg_color(separator, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(separator, 0, LV_PART_MAIN);

    int y_pos = 30;

    // 1. Contrast Slider
    lv_obj_t* contrast_label = lv_label_create(screen_);
    lv_label_set_text(contrast_label, "Contrast");
    lv_obj_set_style_text_font(contrast_label, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(contrast_label, LV_ALIGN_TOP_LEFT, 3, y_pos);

    contrast_slider_ = lv_slider_create(screen_);
    lv_slider_set_range(contrast_slider_, 0, 255);
    lv_slider_set_value(contrast_slider_, config_.contrast, LV_ANIM_OFF);
    lv_obj_set_size(contrast_slider_, 140, 10);
    lv_obj_align(contrast_slider_, LV_ALIGN_TOP_LEFT, 67, y_pos + 2);

    contrast_value_label_ = lv_label_create(screen_);
    lv_label_set_text_fmt(contrast_value_label_, "%d", config_.contrast);
    lv_obj_set_style_text_font(contrast_value_label_, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(contrast_value_label_, LV_ALIGN_TOP_RIGHT, -3, y_pos);

    lv_obj_add_event_cb(
        contrast_slider_,
        [](lv_event_t* e) {
          auto* self = static_cast<SaboScreenConfig*>(lv_event_get_user_data(e));
          if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
            self->OnContrastChanged();
          }
        },
        LV_EVENT_VALUE_CHANGED, this);

    y_pos += 25;

    // 2. Temperature Compensation
    lv_obj_t* temp_label = lv_label_create(screen_);
    lv_label_set_text(temp_label, "Temperature Comp.");
    lv_obj_set_style_text_font(temp_label, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(temp_label, LV_ALIGN_TOP_LEFT, 3, y_pos);

    temp_selector_ = lv_dropdown_create(screen_);
    lv_dropdown_set_options(temp_selector_, "Off\n-0.05%/C\n-0.15%/C\n-0.25%/C");
    lv_dropdown_set_selected(temp_selector_, config_.temp_compensation);
    lv_obj_set_size(temp_selector_, 95, 17);
    lv_obj_set_style_text_font(temp_selector_, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(temp_selector_, LV_ALIGN_TOP_RIGHT, -3, y_pos - 2);
    lv_dropdown_set_dir(temp_selector_, LV_DIR_BOTTOM);

    // Hide the dropdown symbol (down arrow) since it may not render correctly
    lv_dropdown_set_symbol(temp_selector_, NULL);

    // Add event callback for value changes
    lv_obj_add_event_cb(
        temp_selector_,
        [](lv_event_t* e) {
          auto* self = static_cast<SaboScreenConfig*>(lv_event_get_user_data(e));
          self->OnTempCompChanged();
        },
        LV_EVENT_VALUE_CHANGED, this);

    y_pos += 25;

    // 3. Auto-Sleep Timer - Label on top, value and slider below
    lv_obj_t* sleep_label = lv_label_create(screen_);
    lv_label_set_text(sleep_label, "Auto-Sleep");
    lv_obj_set_style_text_font(sleep_label, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(sleep_label, LV_ALIGN_TOP_LEFT, 3, y_pos);

    sleep_slider_ = lv_slider_create(screen_);
    lv_slider_set_range(sleep_slider_, 1, 20);
    lv_slider_set_value(sleep_slider_, config_.auto_sleep_minutes, LV_ANIM_OFF);
    lv_obj_set_size(sleep_slider_, 110, 10);
    lv_obj_align(sleep_slider_, LV_ALIGN_TOP_LEFT, 83, y_pos + 2);

    sleep_value_label_ = lv_label_create(screen_);
    UpdateSleepTimerLabel();
    lv_obj_set_style_text_font(sleep_value_label_, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(sleep_value_label_, LV_ALIGN_TOP_RIGHT, -3, y_pos);

    lv_obj_add_event_cb(
        sleep_slider_,
        [](lv_event_t* e) {
          auto* self = static_cast<SaboScreenConfig*>(lv_event_get_user_data(e));
          if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
            self->OnSleepTimerChanged();
          }
        },
        LV_EVENT_VALUE_CHANGED, this);
  }

  void AddToGroup(lv_group_t* group) {
    if (!group) {
      return;
    }

    // Add all interactive elements to the group
    lv_group_add_obj(group, contrast_slider_);
    lv_group_add_obj(group, temp_selector_);
    lv_group_add_obj(group, sleep_slider_);

    // Focus first element
    if (contrast_slider_) {
      lv_group_focus_obj(contrast_slider_);
    }
  }

  void RemoveFromGroup() {
    if (contrast_slider_) lv_group_remove_obj(contrast_slider_);
    if (temp_selector_) lv_group_remove_obj(temp_selector_);
    if (sleep_slider_) lv_group_remove_obj(sleep_slider_);
  }

  Config& GetConfig() {
    return config_;
  }

  void SetConfig(const Config& cfg) {
    config_ = cfg;

    // Update UI elements
    if (contrast_slider_) {
      lv_slider_set_value(contrast_slider_, config_.contrast, LV_ANIM_OFF);
    }
    if (contrast_value_label_) {
      lv_label_set_text_fmt(contrast_value_label_, "%d", config_.contrast);
    }
    if (temp_selector_) {
      lv_dropdown_set_selected(temp_selector_, config_.temp_compensation);
    }
    if (sleep_slider_) {
      lv_slider_set_value(sleep_slider_, config_.auto_sleep_minutes, LV_ANIM_OFF);
    }
    if (sleep_value_label_) {
      UpdateSleepTimerLabel();
    }
  }

  // Callbacks for when settings change - to be connected to LCD driver
  void SetOnContrastChangedCallback(void (*callback)(uint8_t contrast)) {
    on_contrast_changed_ = callback;
  }

  void SetOnTempCompChangedCallback(void (*callback)(uint8_t temp_comp)) {
    on_temp_comp_changed_ = callback;
  }

  void SetOnSleepTimerChangedCallback(void (*callback)(uint8_t minutes)) {
    on_sleep_timer_changed_ = callback;
  }

  void SetOnSaveCallback(void (*callback)(const Config& config)) {
    on_save_ = callback;
  }

  // Call this when BACK button is pressed
  void OnBackButton() {
    if (on_save_) {
      on_save_(config_);
    }
  }

 private:
  void OnContrastChanged() {
    config_.contrast = lv_slider_get_value(contrast_slider_);
    lv_label_set_text_fmt(contrast_value_label_, "%d", config_.contrast);

    if (on_contrast_changed_) {
      on_contrast_changed_(config_.contrast);
    }
  }

  void OnTempCompChanged() {
    config_.temp_compensation = lv_dropdown_get_selected(temp_selector_);

    if (on_temp_comp_changed_) {
      on_temp_comp_changed_(config_.temp_compensation);
    }
  }

  void OnSleepTimerChanged() {
    config_.auto_sleep_minutes = lv_slider_get_value(sleep_slider_);
    UpdateSleepTimerLabel();

    if (on_sleep_timer_changed_) {
      on_sleep_timer_changed_(config_.auto_sleep_minutes);
    }
  }

  void UpdateSleepTimerLabel() {
    if (config_.auto_sleep_minutes == 0) {
      lv_label_set_text(sleep_value_label_, "Off");
    } else {
      lv_label_set_text_fmt(sleep_value_label_, "%d min", config_.auto_sleep_minutes);
    }
  }

  Config config_;

  // UI elements
  lv_obj_t* contrast_slider_ = nullptr;
  lv_obj_t* contrast_value_label_ = nullptr;
  lv_obj_t* temp_selector_ = nullptr;
  lv_obj_t* sleep_slider_ = nullptr;
  lv_obj_t* sleep_value_label_ = nullptr;

  // Callbacks
  void (*on_contrast_changed_)(uint8_t contrast) = nullptr;
  void (*on_temp_comp_changed_)(uint8_t temp_comp) = nullptr;
  void (*on_sleep_timer_changed_)(uint8_t minutes) = nullptr;
  void (*on_save_)(const Config& config) = nullptr;
};

}  // namespace xbot::driver::ui::lvgl::sabo

#endif  // LVGL_SABO_SCREEN_CONFIG_HPP_
