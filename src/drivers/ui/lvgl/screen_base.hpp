/**
 * @file sabo_lv_screen_main.hpp
 * @brief Main Screen with expandable areas and side menu
 */

#ifndef LVGL_SCREEN_BASE_HPP_
#define LVGL_SCREEN_BASE_HPP_

#include <lvgl.h>

namespace xbot::driver::ui::lvgl {

template <typename ScreenId>
class ScreenBase {
 public:
  explicit ScreenBase(ScreenId screen_id) : screen_id_(screen_id){};
  virtual ~ScreenBase() = default;

  virtual void Create(lv_color_t bg_color = lv_color_white()) {
    screen_ = lv_obj_create(NULL);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen_, bg_color, LV_PART_MAIN);
  };

  virtual void Show() {
    if (screen_) lv_screen_load(screen_);
  };

  ScreenId GetScreenId() const {
    return screen_id_;
  }

  lv_obj_t* GetLvScreen() const {
    return screen_;
  }

 protected:
  lv_obj_t* screen_ = nullptr;
  ScreenId screen_id_;
};

}  // namespace xbot::driver::ui::lvgl

#endif  // LVGL_SCREEN_BASE_HPP_
