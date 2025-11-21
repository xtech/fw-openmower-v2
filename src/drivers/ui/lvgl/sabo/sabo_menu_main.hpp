/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sabo_menu_main.hpp
 * @brief Main menu overlay that slides in from the right
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-11-10
 */

#ifndef LVGL_SABO_MENU_MAIN_HPP_
#define LVGL_SABO_MENU_MAIN_HPP_

#include <lvgl.h>

#include "../../SaboCoverUI/sabo_cover_ui_defs.hpp"

extern "C" {
LV_FONT_DECLARE(orbitron_12);
LV_FONT_DECLARE(orbitron_16b);
}

namespace xbot::driver::ui::lvgl::sabo {

using namespace xbot::driver::ui::sabo::display;  // For LCD_WIDTH, LCD_HEIGHT

/**
 * @brief Main menu overlay - slides in from the right side
 *
 * This is NOT a full screen but an overlay widget that can be attached to any parent screen.
 * It provides navigation to other screens like Settings, Status, Command, About.
 */
class SaboMenuMain {
 public:
  static const int MENU_ITEMS = 4;
  enum class MenuItem { CMD, INPUTS, SETTINGS, ABOUT };
  enum class AnimationState { HIDDEN, SLIDING_IN, VISIBLE, SLIDING_OUT };

  using MenuClosedCallback = void (*)(void* context);

  SaboMenuMain() = default;

  ~SaboMenuMain() {
    // Clear the closed callback to prevent it from firing during destruction
    closed_callback_ = nullptr;
    closed_callback_context_ = nullptr;

    if (menu_anim_) {
      lv_anim_delete(menu_container_, nullptr);
      delete menu_anim_;
      menu_anim_ = nullptr;
    }

    // Delete LVGL container object (this also deletes all children)
    if (menu_container_) {
      lv_obj_delete(menu_container_);
      menu_container_ = nullptr;
    }
  }

  /**
   * @brief Set callback to be called when menu closes (after slide-out animation completes)
   * @param callback Function pointer to call on completion
   * @param context User context pointer passed to callback
   */
  void SetClosedCallback(MenuClosedCallback callback, void* context = nullptr) {
    closed_callback_ = callback;
    closed_callback_context_ = context;
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
    lv_obj_set_style_border_width(menu_container_, 1, LV_PART_MAIN);  // Thinner border (1px instead of 2px)
    lv_obj_set_style_border_color(menu_container_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(menu_container_, 0, LV_PART_MAIN);  // Remove internal padding
    lv_obj_clear_flag(menu_container_, LV_OBJ_FLAG_SCROLLABLE);

    // Start hidden (off-screen to the right)
    lv_obj_set_pos(menu_container_, LCD_WIDTH, 0);

    // Menu title - larger bold font
    lv_obj_t* title_label = lv_label_create(menu_container_);
    lv_label_set_text(title_label, "Menu");
    lv_obj_set_style_text_color(title_label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title_label, &orbitron_16b, LV_PART_MAIN);  // Bold font
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 3);

    // Horizontal separator line below title - full width
    lv_obj_t* separator = lv_obj_create(menu_container_);
    lv_obj_set_size(separator, LCD_WIDTH / 3, 1);  // Full width, no margins
    lv_obj_set_pos(separator, 0, 23);              // Just below title, flush left
    lv_obj_set_style_bg_color(separator, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(separator, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(separator, 0, LV_PART_MAIN);

    // Create menu items (starting after separator)
    CreateMenuItem(MenuItem::CMD, "Command", 35);
    CreateMenuItem(MenuItem::INPUTS, "Inputs", 60);
    CreateMenuItem(MenuItem::SETTINGS, "Settings", 85);
    CreateMenuItem(MenuItem::ABOUT, "About", 110);

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
      auto* self = static_cast<SaboMenuMain*>(var);
      lv_obj_set_x(self->menu_container_, value);
    });
    lv_anim_set_path_cb(menu_anim_, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(menu_anim_, [](lv_anim_t* anim) {
      auto* self = static_cast<SaboMenuMain*>(anim->var);
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
      auto* self = static_cast<SaboMenuMain*>(var);
      lv_obj_set_x(self->menu_container_, value);
    });
    lv_anim_set_path_cb(menu_anim_, lv_anim_path_ease_in);
    lv_anim_set_completed_cb(menu_anim_, [](lv_anim_t* anim) {
      auto* self = static_cast<SaboMenuMain*>(anim->var);
      self->anim_state_ = AnimationState::HIDDEN;
      lv_obj_add_flag(self->menu_container_, LV_OBJ_FLAG_HIDDEN);

      // Call closed callback if set
      if (self->closed_callback_) {
        self->closed_callback_(self->closed_callback_context_);
      }
    });

    anim_state_ = AnimationState::SLIDING_OUT;
    lv_anim_start(menu_anim_);
  }

  AnimationState GetAnimationState() const {
    return anim_state_;
  }

  void SetMenuItemCallback(MenuItem item, lv_event_cb_t callback, void* user_data = nullptr) {
    if (menu_items_[(int)item]) {
      lv_obj_add_event_cb(menu_items_[(int)item], callback, LV_EVENT_CLICKED, user_data);
    }
  }

  /**
   * @brief Add menu items to an LVGL group for keypad navigation
   * @param group The group to add items to
   */
  void AddToGroup(lv_group_t* group) {
    if (!group) {
      return;
    }

    for (int i = 0; i < MENU_ITEMS; i++) {
      if (menu_items_[i]) {
        lv_group_add_obj(group, menu_items_[i]);
      }
    }

    // Focus first item after adding to group
    if (menu_items_[0]) {
      lv_group_focus_obj(menu_items_[0]);
    }
  } /**
     * @brief Remove menu items from their LVGL group
     */
  void RemoveFromGroup() {
    for (int i = 0; i < MENU_ITEMS; i++) {
      if (menu_items_[i]) {
        lv_group_remove_obj(menu_items_[i]);
      }
    }
  }

 private:
  lv_obj_t* menu_container_ = nullptr;
  lv_anim_t* menu_anim_ = nullptr;
  AnimationState anim_state_ = AnimationState::HIDDEN;
  lv_obj_t* menu_items_[MENU_ITEMS] = {nullptr};  // CMD, INPUTS, SETTINGS, ABOUT

  MenuClosedCallback closed_callback_ = nullptr;
  void* closed_callback_context_ = nullptr;

  void CreateMenuItem(MenuItem item, const char* text, int y_offset) {
    // Create button with flat styling for non-touch display
    lv_obj_t* btn = lv_button_create(menu_container_);
    lv_obj_set_size(btn, LCD_WIDTH / 3 - 10, 20);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, y_offset);

    // Flat button style: no shadow, no 3D effect
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_make(0xD0, 0xD0, 0xD0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 0, LV_PART_MAIN);

    // Focused style: inverted (white text on black)
    lv_obj_set_style_bg_color(btn, lv_color_black(), LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_FOCUSED);

    // Create label inside button
    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    lv_obj_set_style_text_color(label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &orbitron_12, LV_PART_MAIN);

    // Add event callback to handle focus state changes and clicks
    lv_obj_add_event_cb(
        btn,
        [](lv_event_t* e) {
          lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
          lv_obj_t* label = lv_obj_get_child(btn, 0);
          lv_event_code_t code = lv_event_get_code(e);

          if (code == LV_EVENT_FOCUSED) {
            // Change label to white when button focused
            lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
          } else if (code == LV_EVENT_DEFOCUSED) {
            // Change label back to black when button defocused
            lv_obj_set_style_text_color(label, lv_color_black(), LV_PART_MAIN);
          } else if (code == LV_EVENT_CLICKED) {
            const char* text = lv_label_get_text(label);
            ULOG_INFO("Menu item clicked: %s", text);
          }
        },
        LV_EVENT_ALL, nullptr);

    menu_items_[(int)item] = btn;
  }
};

}  // namespace xbot::driver::ui::lvgl::sabo

#endif  // LVGL_SABO_MENU_MAIN_HPP_
