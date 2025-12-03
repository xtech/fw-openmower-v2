/**
 * @file sabo_lv_screen_main.hpp
 * @brief Main Screen with expandable areas and side menu
 */

#ifndef LVGL_SABO_SCREEN_MAIN_HPP_
#define LVGL_SABO_SCREEN_MAIN_HPP_

#include <lvgl.h>

#include "sabo_defs.hpp"
#include "screen_base.hpp"

extern "C" {
LV_FONT_DECLARE(orbitron_12);
}

namespace xbot::driver::ui::lvgl::sabo {

class SaboScreenMain : public ScreenBase<sabo::ScreenId> {
 public:
  SaboScreenMain() : ScreenBase<sabo::ScreenId>(sabo::ScreenId::MAIN) {
  }

  void Create(lv_color_t bg_color = lv_color_white()) override {
    ScreenBase::Create(bg_color);

    lv_obj_t* title_label = lv_label_create(screen_);
    lv_label_set_text(title_label, "ToDo: MainScreen @ SABO");
    lv_obj_set_style_text_color(title_label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title_label, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_CENTER, 0, 0);
  }

 private:
};

}  // namespace xbot::driver::ui::lvgl::sabo

#endif  // LVGL_SABO_SCREEN_MAIN_HPP_
