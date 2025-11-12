/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sabo_screen_about.hpp
 * @brief About Screen with information and credits
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-11-11
 */

#ifndef LVGL_SABO_SCREEN_ABOUT_HPP_
#define LVGL_SABO_SCREEN_ABOUT_HPP_

#include <lvgl.h>
#include <ulog.h>

#include "../../SaboCoverUI/sabo_cover_ui_controller.hpp"
#include "../../SaboCoverUI/sabo_cover_ui_defs.hpp"
#include "../screen_base.hpp"
#include "git_version.h"
#include "globals.hpp"
#include "sabo_defs.hpp"

extern "C" {
LV_FONT_DECLARE(orbitron_12);
LV_FONT_DECLARE(orbitron_16b);
}

namespace xbot::driver::ui::lvgl::sabo {

class SaboScreenAbout : public ScreenBase<ScreenId, xbot::driver::ui::sabo::ButtonID> {
 public:
  SaboScreenAbout() : ScreenBase<ScreenId, xbot::driver::ui::sabo::ButtonID>(sabo::ScreenId::ABOUT) {
  }

  ~SaboScreenAbout() {
    // LVGL objects are automatically deleted with the screen, but we clean up explicitly
    if (content_container_) {
      lv_obj_delete(content_container_);
      content_container_ = nullptr;
    }
    if (screen_) {
      lv_obj_delete(screen_);
      screen_ = nullptr;
    }
  }

  void Create(lv_color_t bg_color = lv_color_white()) override {
    ScreenBase::Create(bg_color);

    // Title (stays at top, NOT scrollable)
    lv_obj_t* title = lv_label_create(screen_);
    lv_label_set_text(title, "SABO @ OpenMower V2");
    lv_obj_set_style_text_font(title, &orbitron_16b, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);

    // Separator line (stays at top, NOT scrollable)
    lv_obj_t* separator = lv_obj_create(screen_);
    lv_obj_set_size(separator, display::LCD_WIDTH, 1);
    lv_obj_set_pos(separator, 0, 25);
    lv_obj_set_style_bg_color(separator, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(separator, 0, LV_PART_MAIN);

    // Scrollable content container
    lv_obj_t* content_container = lv_obj_create(screen_);
    lv_obj_set_size(content_container, display::LCD_WIDTH, display::LCD_HEIGHT - 30);
    lv_obj_set_pos(content_container, 0, 30);
    lv_obj_add_flag(content_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(content_container, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(content_container, LV_DIR_VER);
    lv_obj_set_style_bg_color(content_container, bg_color, LV_PART_MAIN);
    lv_obj_set_style_border_width(content_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(content_container, 0, LV_PART_MAIN);

    int y_pos = 5;

    // Firmware Section - Header
    lv_obj_t* fw_header = lv_label_create(content_container);
    lv_label_set_text(fw_header, "Firmware:");
    lv_obj_set_style_text_font(fw_header, &orbitron_16b, LV_PART_MAIN);
    lv_obj_set_style_text_color(fw_header, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_pos(fw_header, 5, y_pos);

    y_pos += 18;

    // Firmware Section - Details
    char fw_text[256];
    snprintf(fw_text, sizeof(fw_text),
             "- Built: %s\n"
             "- Git: %s",
             BUILD_DATE, BUILD_GIT_HASH);
    lv_obj_t* fw_label = lv_label_create(content_container);
    lv_label_set_text(fw_label, fw_text);
    lv_obj_set_style_text_color(fw_label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(fw_label, &orbitron_12, LV_PART_MAIN);
    lv_obj_set_style_text_align(fw_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_pos(fw_label, 5, y_pos);

    y_pos += 40;

    // Hardware Section - Header
    lv_obj_t* hw_header = lv_label_create(content_container);
    lv_label_set_text(hw_header, "Hardware:");
    lv_obj_set_style_text_font(hw_header, &orbitron_16b, LV_PART_MAIN);
    lv_obj_set_style_text_color(hw_header, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_pos(hw_header, 5, y_pos);

    y_pos += 18;

    // Hardware Section - Details
    char hw_text[256];
    snprintf(hw_text, sizeof(hw_text),
             "- %s v%d.%d.%d\n"
             "- %s v%d.%d.%d",
             carrier_board_info.board_id, carrier_board_info.version_major, carrier_board_info.version_minor,
             carrier_board_info.version_patch, board_info.board_id, board_info.version_major, board_info.version_minor,
             board_info.version_patch);
    lv_obj_t* hw_label = lv_label_create(content_container);
    lv_label_set_text(hw_label, hw_text);
    lv_obj_set_style_text_color(hw_label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(hw_label, &orbitron_12, LV_PART_MAIN);
    lv_obj_set_style_text_align(hw_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_pos(hw_label, 5, y_pos);

    y_pos += 40;

    // Credits Section - Header
    lv_obj_t* credits_header = lv_label_create(content_container);
    lv_label_set_text(credits_header, "Credits:");
    lv_obj_set_style_text_font(credits_header, &orbitron_16b, LV_PART_MAIN);
    lv_obj_set_style_text_color(credits_header, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_pos(credits_header, 5, y_pos);

    y_pos += 18;

    // Credits Section - Details
    char credits_text[256];
    snprintf(credits_text, sizeof(credits_text),
             "x-tech and the\n"
             "OpenMower Contributors\n"
             "https://github.com/xtech/\nfw-openmower-v2");
    lv_obj_t* credits_label = lv_label_create(content_container);
    lv_label_set_text(credits_label, credits_text);
    lv_obj_set_style_text_color(credits_label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(credits_label, &orbitron_12, LV_PART_MAIN);
    lv_obj_set_style_text_align(credits_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_pos(credits_label, 5, y_pos);

    // Store content container for use in OnButtonPress
    content_container_ = content_container;
  }

  /**
   * @brief Activate screen for input focus - adds content container to LVGL group
   * This enables scrolling through the LVGL group instead of direct button handling
   * When group is deactivated (e.g., menu opens), scrolling is automatically disabled
   */
  void Activate(lv_group_t* group) override {
    ScreenBase::Activate(group);
    if (group && content_container_) {
      lv_group_add_obj(group, content_container_);
      lv_group_focus_obj(content_container_);
    }
  }

  /**
   * @brief Deactivate screen input focus - removes content container from LVGL group
   */
  void Deactivate() override {
    if (content_container_) {
      lv_group_remove_obj(content_container_);
    }
    ScreenBase::Deactivate();
  }

  /**
   * @brief Handle button presses - scroll content when UP/DOWN pressed
   * This is called by the display controller when a button is pressed
   * Only receives input when the group is active (menu is closed)
   */
  bool OnButtonPress(xbot::driver::ui::sabo::ButtonID button_id) override {
    using ButtonID = xbot::driver::ui::sabo::ButtonID;

    if (!content_container_) {
      return false;
    }

    switch (button_id) {
      case ButtonID::UP: lv_obj_scroll_by_bounded(content_container_, 0, 20, LV_ANIM_OFF); return true;
      case ButtonID::DOWN: lv_obj_scroll_by_bounded(content_container_, 0, -20, LV_ANIM_OFF); return true;
      default: return false;
    }
  }

 private:
  lv_obj_t* content_container_ = nullptr;
};

}  // namespace xbot::driver::ui::lvgl::sabo

#endif  // LVGL_SABO_SCREEN_MAIN_HPP_
