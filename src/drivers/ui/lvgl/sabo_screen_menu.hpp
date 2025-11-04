/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sabo_screen_menu.hpp
 * @brief Side menu that slides in from the right
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-11-03
 */

#ifndef LVGL_SABO_SCREEN_MENU_HPP_
#define LVGL_SABO_SCREEN_MENU_HPP_

#include <lvgl.h>

#include "../SaboCoverUI/sabo_cover_ui_defs.hpp"
#include "sabo_defs.hpp"
#include "screen_base.hpp"

extern "C" {
LV_FONT_DECLARE(orbitron_12);
}

namespace xbot::driver::ui::lvgl::sabo {

using namespace xbot::driver::ui::sabo::display;  // For LCD_WIDTH, LCD_HEIGHT

class SaboScreenMenu : public ScreenBase<ScreenId, xbot::driver::ui::sabo::ButtonID> {
 public:
  SaboScreenMenu() : ScreenBase<ScreenId, xbot::driver::ui::sabo::ButtonID>(sabo::ScreenId::MENU) {
  }

  ~SaboScreenMenu() {
    if (menu_anim_) {
      lv_anim_delete(menu_container_, nullptr);
      delete menu_anim_;
      menu_anim_ = nullptr;
    }
  }

  enum class AnimationState { HIDDEN, SLIDING_IN, VISIBLE, SLIDING_OUT };

  enum class MenuItem { CMD, STATUS, CONFIG, ABOUT };

  void Create(lv_color_t bg_color = lv_color_white()) override {
    ScreenBase::Create(bg_color);

    // Menu container (1/3 screen width, full height)
    menu_container_ = lv_obj_create(screen_);
    lv_obj_set_size(menu_container_, LCD_WIDTH / 3, LCD_HEIGHT);
    lv_obj_set_style_bg_color(menu_container_, lv_color_make(0xE0, 0xE0, 0xE0), LV_PART_MAIN);
    lv_obj_set_style_border_width(menu_container_, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(menu_container_, lv_color_black(), LV_PART_MAIN);
    lv_obj_clear_flag(menu_container_, LV_OBJ_FLAG_SCROLLABLE);

    // Start hidden (off-screen to the right)
    lv_obj_set_pos(menu_container_, LCD_WIDTH, 0);

    // Menu title
    lv_obj_t* title_label = lv_label_create(menu_container_);
    lv_label_set_text(title_label, "Menu");
    lv_obj_set_style_text_color(title_label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title_label, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 5);

    // Create menu items
    CreateMenuItem(MenuItem::CMD, "Command", 30);
    CreateMenuItem(MenuItem::STATUS, "Status", 60);
    CreateMenuItem(MenuItem::CONFIG, "Config", 90);
    CreateMenuItem(MenuItem::ABOUT, "About", 120);

    anim_state_ = AnimationState::HIDDEN;
  }

  /**
   * @brief Create menu as overlay on an existing screen
   * @param parent_screen The screen to attach the menu to
   */
  void CreateOverlay(lv_obj_t* parent_screen) {
    if (!parent_screen) {
      return;
    }

    // Menu container (1/3 screen width, full height) attached to parent screen
    menu_container_ = lv_obj_create(parent_screen);
    lv_obj_set_size(menu_container_, LCD_WIDTH / 3, LCD_HEIGHT);
    lv_obj_set_style_bg_color(menu_container_, lv_color_make(0xE0, 0xE0, 0xE0), LV_PART_MAIN);
    lv_obj_set_style_border_width(menu_container_, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(menu_container_, lv_color_black(), LV_PART_MAIN);
    lv_obj_clear_flag(menu_container_, LV_OBJ_FLAG_SCROLLABLE);

    // Start hidden (off-screen to the right)
    lv_obj_set_pos(menu_container_, LCD_WIDTH, 0);

    // Menu title
    lv_obj_t* title_label = lv_label_create(menu_container_);
    lv_label_set_text(title_label, "Menu");
    lv_obj_set_style_text_color(title_label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title_label, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 5);

    // Create menu items
    CreateMenuItem(MenuItem::CMD, "Command", 30);
    CreateMenuItem(MenuItem::STATUS, "Status", 60);
    CreateMenuItem(MenuItem::CONFIG, "Config", 90);
    CreateMenuItem(MenuItem::ABOUT, "About", 120);

    anim_state_ = AnimationState::HIDDEN;
  }

  void SlideIn(uint32_t duration_ms = 300) {
    if (anim_state_ == AnimationState::VISIBLE || anim_state_ == AnimationState::SLIDING_IN) {
      return;
    }

    lv_obj_clear_flag(menu_container_, LV_OBJ_FLAG_HIDDEN);

    if (!menu_anim_) {
      menu_anim_ = new lv_anim_t;
    }

    lv_anim_init(menu_anim_);
    lv_anim_set_var(menu_anim_, this);
    lv_anim_set_values(menu_anim_, LCD_WIDTH, LCD_WIDTH * 2 / 3);  // Slide to 2/3 position (1/3 visible)
    lv_anim_set_duration(menu_anim_, duration_ms);
    lv_anim_set_exec_cb(menu_anim_, [](void* var, int32_t value) {
      auto* self = static_cast<SaboScreenMenu*>(var);
      lv_obj_set_x(self->menu_container_, value);
    });
    lv_anim_set_path_cb(menu_anim_, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(menu_anim_, [](lv_anim_t* anim) {
      auto* self = static_cast<SaboScreenMenu*>(anim->var);
      self->anim_state_ = AnimationState::VISIBLE;
    });

    anim_state_ = AnimationState::SLIDING_IN;
    lv_anim_start(menu_anim_);
  }

  void SlideOut(uint32_t duration_ms = 300) {
    if (anim_state_ == AnimationState::HIDDEN || anim_state_ == AnimationState::SLIDING_OUT) {
      return;
    }

    if (!menu_anim_) {
      menu_anim_ = new lv_anim_t;
    }

    lv_anim_init(menu_anim_);
    lv_anim_set_var(menu_anim_, this);
    lv_anim_set_values(menu_anim_, lv_obj_get_x(menu_container_), LCD_WIDTH);  // Slide off-screen
    lv_anim_set_duration(menu_anim_, duration_ms);
    lv_anim_set_exec_cb(menu_anim_, [](void* var, int32_t value) {
      auto* self = static_cast<SaboScreenMenu*>(var);
      lv_obj_set_x(self->menu_container_, value);
    });
    lv_anim_set_path_cb(menu_anim_, lv_anim_path_ease_in);
    lv_anim_set_completed_cb(menu_anim_, [](lv_anim_t* anim) {
      auto* self = static_cast<SaboScreenMenu*>(anim->var);
      self->anim_state_ = AnimationState::HIDDEN;
      lv_obj_add_flag(self->menu_container_, LV_OBJ_FLAG_HIDDEN);
    });

    anim_state_ = AnimationState::SLIDING_OUT;
    lv_anim_start(menu_anim_);
  }

  void Toggle(uint32_t duration_ms = 300) {
    if (anim_state_ == AnimationState::VISIBLE || anim_state_ == AnimationState::SLIDING_IN) {
      SlideOut(duration_ms);
    } else {
      SlideIn(duration_ms);
    }
  }

  AnimationState GetAnimationState() const {
    return anim_state_;
  }

  void SetMenuItemCallback(MenuItem item, lv_event_cb_t callback, void* user_data = nullptr) {
    if (menu_items_[(int)item]) {
      lv_obj_add_event_cb(menu_items_[(int)item], callback, LV_EVENT_CLICKED, user_data);
    }
  }

 private:
  void CreateMenuItem(MenuItem item, const char* text, int y_offset) {
    lv_obj_t* btn = lv_button_create(menu_container_);
    lv_obj_set_size(btn, LCD_WIDTH / 3 - 10, 25);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, y_offset);
    lv_obj_set_style_bg_color(btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_make(0xC0, 0xC0, 0xC0), LV_STATE_PRESSED);

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &orbitron_12, LV_PART_MAIN);
    lv_obj_center(label);

    menu_items_[(int)item] = btn;
  }

  lv_obj_t* menu_container_ = nullptr;
  lv_obj_t* menu_items_[4] = {nullptr};  // CMD, STATUS, CONFIG, ABOUT
  lv_anim_t* menu_anim_ = nullptr;
  AnimationState anim_state_ = AnimationState::HIDDEN;
};

}  // namespace xbot::driver::ui::lvgl::sabo

#endif  // LVGL_SABO_SCREEN_MENU_HPP_
