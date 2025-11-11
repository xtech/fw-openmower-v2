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
#include "sabo_defs.hpp"

extern "C" {
LV_FONT_DECLARE(orbitron_12);
}

namespace xbot::driver::ui::lvgl::sabo {

class SaboScreenAbout : public ScreenBase<ScreenId, xbot::driver::ui::sabo::ButtonID> {
 public:
  SaboScreenAbout() : ScreenBase<ScreenId, xbot::driver::ui::sabo::ButtonID>(sabo::ScreenId::MAIN) {
  }

  void Create(lv_color_t bg_color = lv_color_white()) override {
    ScreenBase::Create(bg_color);

    status_label_ = lv_label_create(screen_);
    lv_label_set_text(status_label_, "ToDo: Main Screen\n\nPress any key for test");
    lv_obj_set_style_text_color(status_label_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(status_label_, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(status_label_, LV_ALIGN_CENTER, 0, 0);
  }

  /**
   * @brief Handle button presses on main screen
   * @return true if button was handled, false to pass to global handler
   */
  bool OnButtonPress(xbot::driver::ui::sabo::ButtonID button_id) override {
    using ButtonID = xbot::driver::ui::sabo::ButtonID;

    // Example button handling
    switch (button_id) {
      case ButtonID::OK:
        ULOG_INFO("Main screen: OK button pressed");
        if (status_label_) {
          lv_label_set_text(status_label_, "OK pressed!");
        }
        return true;  // Button was handled

      default:
        if (status_label_) {
          lv_label_set_text(status_label_, xbot::driver::ui::sabo::ButtonIDToString(button_id));
        }
        return false;  // Let global handler deal with it (e.g., MENU button)
    }
  }

 private:
  lv_obj_t* status_label_ = nullptr;
};

}  // namespace xbot::driver::ui::lvgl::sabo

#endif  // LVGL_SABO_SCREEN_MAIN_HPP_
