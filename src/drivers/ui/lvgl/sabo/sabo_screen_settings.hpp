/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sabo_screen_settings.hpp
 * @brief Settings screen for LCD adjustments (Contrast, Temp. Compensation, Auto-Sleep)
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-11-07
 */

#ifndef LVGL_SABO_SCREEN_SETTINGS_HPP_
#define LVGL_SABO_SCREEN_SETTINGS_HPP_

#include <lvgl.h>
#include <ulog.h>

#include "../../SaboCoverUI/sabo_cover_ui_defs.hpp"
#include "../../SaboCoverUI/sabo_cover_ui_display_driver_uc1698.hpp"
#include "../screen_base.hpp"

extern "C" {
LV_FONT_DECLARE(orbitron_12);
LV_FONT_DECLARE(orbitron_16b);
}

namespace xbot::driver::ui::lvgl::sabo {

using namespace xbot::driver::ui::sabo;
using namespace xbot::driver::ui::sabo::settings;
using DriverUC1698 = SaboCoverUIDisplayDriverUC1698;

class SaboScreenSettings : public ScreenBase<ScreenId, ButtonID> {
 public:
  SaboScreenSettings() : ScreenBase<ScreenId, ButtonID>(ScreenId::SETTINGS) {
    if (!xbot::driver::filesystem::VersionedStruct<LCDSettings>::Load(settings_)) {
      ULOG_WARNING("Failed loading settings from %s, using default settings", LCDSettings::PATH);
    }
  }

  ~SaboScreenSettings() {
    // LVGL-Objekte explizit löschen
    if (contrast_slider_) lv_obj_delete(contrast_slider_);
    if (contrast_value_label_) lv_obj_delete(contrast_value_label_);
    if (temp_selector_) lv_obj_delete(temp_selector_);
    if (sleep_slider_) lv_obj_delete(sleep_slider_);
    if (sleep_value_label_) lv_obj_delete(sleep_value_label_);

    // Screen löschen (falls erstellt)
    if (screen_) lv_obj_delete(screen_);
  }

  void Create(lv_color_t bg_color = lv_color_white()) override {
    ScreenBase::Create(bg_color);

    // Title
    lv_obj_t* title = lv_label_create(screen_);
    lv_label_set_text(title, "LCD Configuration");
    lv_obj_set_style_text_font(title, &orbitron_16b, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    // Separator line
    lv_obj_t* separator = lv_obj_create(screen_);
    lv_obj_set_size(separator, display::LCD_WIDTH, 1);
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
    lv_slider_set_value(contrast_slider_, settings_.contrast, LV_ANIM_OFF);
    lv_obj_set_size(contrast_slider_, 140, 10);
    lv_obj_align(contrast_slider_, LV_ALIGN_TOP_LEFT, 67, y_pos + 2);

    contrast_value_label_ = lv_label_create(screen_);
    lv_label_set_text_fmt(contrast_value_label_, "%d", settings_.contrast);
    lv_obj_set_style_text_font(contrast_value_label_, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(contrast_value_label_, LV_ALIGN_TOP_RIGHT, -3, y_pos);

    lv_obj_add_event_cb(
        contrast_slider_,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
            auto* self = static_cast<SaboScreenSettings*>(lv_event_get_user_data(e));
            if (!self) return;
            uint8_t contrast = lv_slider_get_value(self->contrast_slider_);
            self->settings_.contrast = contrast;
            lv_label_set_text_fmt(self->contrast_value_label_, "%d", contrast);
            DriverUC1698::Instance().SetVBiasPotentiometer(contrast);
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
    lv_dropdown_set_selected(temp_selector_, static_cast<uint32_t>(settings_.temp_compensation));
    lv_obj_set_size(temp_selector_, 95, 17);
    lv_obj_set_style_text_font(temp_selector_, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(temp_selector_, LV_ALIGN_TOP_RIGHT, -3, y_pos - 2);
    lv_dropdown_set_dir(temp_selector_, LV_DIR_BOTTOM);

    // Hide the dropdown symbol (down arrow) since we don't have it in our font
    lv_dropdown_set_symbol(temp_selector_, NULL);

    // Add event callback for value changes
    lv_obj_add_event_cb(
        temp_selector_,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
            auto* self = static_cast<SaboScreenSettings*>(lv_event_get_user_data(e));
            if (!self) return;
            auto temp_comp = static_cast<TempCompensation>(lv_dropdown_get_selected(self->temp_selector_));
            self->settings_.temp_compensation = temp_comp;
            DriverUC1698::Instance().SetTemperatureCompensation(static_cast<TempCompensation>(temp_comp));
          }
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
    lv_slider_set_value(sleep_slider_, settings_.auto_sleep_minutes, LV_ANIM_OFF);
    lv_obj_set_size(sleep_slider_, 110, 10);
    lv_obj_align(sleep_slider_, LV_ALIGN_TOP_LEFT, 83, y_pos + 2);

    sleep_value_label_ = lv_label_create(screen_);
    lv_label_set_text_fmt(sleep_value_label_, "%d min", settings_.auto_sleep_minutes);
    lv_obj_set_style_text_font(sleep_value_label_, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(sleep_value_label_, LV_ALIGN_TOP_RIGHT, -3, y_pos);

    lv_obj_add_event_cb(
        sleep_slider_,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
            auto* self = static_cast<SaboScreenSettings*>(lv_event_get_user_data(e));
            if (!self) return;
            uint8_t sleep_timer = lv_slider_get_value(self->sleep_slider_);
            self->settings_.auto_sleep_minutes = sleep_timer;
            lv_label_set_text_fmt(self->sleep_value_label_, "%d min", sleep_timer);
          }
        },
        LV_EVENT_VALUE_CHANGED, this);
  }

  void Activate(lv_group_t* group) override {
    ScreenBase::Activate(group);
    if (group) {
      // Add all interactive elements to the group
      lv_group_add_obj(group, contrast_slider_);
      lv_group_add_obj(group, temp_selector_);
      lv_group_add_obj(group, sleep_slider_);
    }

    // Focus first element
    lv_group_focus_obj(contrast_slider_);
  }

  void Deactivate() override {
    lv_group_remove_obj(contrast_slider_);
    lv_group_remove_obj(temp_selector_);
    lv_group_remove_obj(sleep_slider_);

    ScreenBase::Deactivate();
  }

  LCDSettings& GetSettings() {
    return settings_;
  }

  bool SaveSettings() {
    return LCDSettings::Save(settings_);
  }

 private:
  LCDSettings settings_;

  // UI elements
  lv_obj_t* contrast_slider_ = nullptr;
  lv_obj_t* contrast_value_label_ = nullptr;
  lv_obj_t* temp_selector_ = nullptr;
  lv_obj_t* sleep_slider_ = nullptr;
  lv_obj_t* sleep_value_label_ = nullptr;
};

}  // namespace xbot::driver::ui::lvgl::sabo

#endif  // LVGL_SABO_SCREEN_SETTINGS_HPP_
