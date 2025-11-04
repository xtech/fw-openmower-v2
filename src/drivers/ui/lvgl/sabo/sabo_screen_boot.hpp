/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LVGL_SABO_SCREEN_BOOT_HPP_
#define LVGL_SABO_SCREEN_BOOT_HPP_

#include <etl/string_view.h>
#include <lvgl.h>

#include "../../SaboCoverUI/sabo_cover_ui_defs.hpp"
#include "../screen_base.hpp"
#include "../widget_textbar.hpp"
#include "sabo_defs.hpp"

extern "C" {
LV_IMG_DECLARE(chassis_193x94x1);
LV_IMG_DECLARE(wheel_83x83x1);
LV_FONT_DECLARE(orbitron_16b);
LV_FONT_DECLARE(orbitron_12);
}

namespace xbot::driver::ui::lvgl::sabo {

class SaboScreenBoot : public ScreenBase<ScreenId, xbot::driver::ui::sabo::ButtonID> {
 public:
  SaboScreenBoot() : ScreenBase<ScreenId, xbot::driver::ui::sabo::ButtonID>(sabo::ScreenId::BOOT) {
  }

  ~SaboScreenBoot() {
  }

  enum class AnimationState { NONE, STARTED, DONE };

  void Create(lv_color_t bg_color = lv_color_white()) override {
    ScreenBase::Create(bg_color);

    // Title
    lv_obj_t* title_label = lv_label_create(screen_);
    lv_label_set_text(title_label, "OpenMower @ SABO");
    lv_obj_set_style_text_color(title_label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title_label, &orbitron_16b, LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 0);

    // Mower (chassis and wheel) container
    mower_ = lv_obj_create(screen_);
    lv_obj_set_size(mower_, 220, 99);  // mower + wheel size
    lv_obj_clear_flag(mower_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(mower_, 0, LV_PART_MAIN);
    lv_obj_align(mower_, LV_ALIGN_CENTER, 0, 0);

    // Chassis
    lv_obj_t* chassis = lv_img_create(mower_);
    lv_img_set_src(chassis, &chassis_193x94x1);
    lv_obj_align(chassis, LV_ALIGN_CENTER, -14, -2);

    // Wheel
    wheel_ = lv_img_create(mower_);
    lv_img_set_src(wheel_, &wheel_83x83x1);
    lv_obj_align_to(wheel_, chassis, LV_ALIGN_CENTER, 83, 10);  // Position relative to chassis
    lv_obj_set_style_transform_pivot_x(wheel_, 41, 0);          // Center of 83x83 image
    lv_obj_set_style_transform_pivot_y(wheel_, 41, 0);          // Center of 83x83 image

    // Status- TextBar
    status_bar_ = new WidgetTextBar(screen_, "Initializing ...", LV_ALIGN_BOTTOM_MID, 0, 0, lv_obj_get_width(screen_),
                                    20, &orbitron_12);
  }

  void SetBootStatus(const etl::string_view& text, int progress) {
    if (status_bar_) {
      status_bar_->SetValue(progress, text.data());
    }
  }

  void StartForwardAnimation() {
    if (!mower_) return;

    animation_state_ = AnimationState::STARTED;

    // Get screen width to calculate exit position
    int32_t screen_width = lv_obj_get_width(screen_);
    int32_t container_width = lv_obj_get_width(mower_);
    int32_t exit_x = -(container_width) - ((screen_width - container_width) / 2);  // Completely off-screen left

    // Create horizontal animation
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, this);
    lv_anim_set_values(&anim, 0, exit_x);
    lv_anim_set_time(&anim, 3000);  // Time to drive off
    lv_anim_set_repeat_count(&anim, 1);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in);      // Start slow, then accelerate
    lv_anim_set_exec_cb(&anim, [](void* var, int32_t v) {  // Animator callback
      auto* self = static_cast<SaboScreenBoot*>(var);
      lv_obj_align(self->mower_, LV_ALIGN_CENTER, v, 0);
    });
    lv_anim_set_ready_cb(&anim, [](lv_anim_t* a) {
      auto* self = static_cast<SaboScreenBoot*>(a->var);
      lv_anim_delete(self->wheel_anim_, self->wheel_anim_->exec_cb);
    });
    lv_anim_set_deleted_cb(&anim, [](lv_anim_t* a) {
      auto* self = static_cast<SaboScreenBoot*>(a->var);
      self->animation_state_ = AnimationState::DONE;
      self->mower_anim_ = nullptr;  // Clear reference when deleted
    });

    StartWheelAnimation();               // Let's rotate the wheel
    mower_anim_ = lv_anim_start(&anim);  // Start moving
  }

  void StartWheelAnimation() {
    if (!wheel_) return;

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, wheel_);
    lv_anim_set_values(&anim, 3600, 0);  // CCW
    lv_anim_set_time(&anim, 3000);       // Time for full rotation
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in);  // Start slow, then accelerate
    lv_anim_set_exec_cb(
        &anim, [](void* var, int32_t v) { lv_obj_set_style_transform_angle(static_cast<lv_obj_t*>(var), v, 0); });
    lv_anim_set_completed_cb(&anim, [](lv_anim_t* a) {
      auto* self = static_cast<SaboScreenBoot*>(a->var);
      self->wheel_anim_ = nullptr;  // Clear reference when done
    });
    lv_anim_set_deleted_cb(&anim, [](lv_anim_t* a) {
      auto* self = static_cast<SaboScreenBoot*>(a->var);
      self->wheel_anim_ = nullptr;  // Clear reference when deleted
    });
    wheel_anim_ = lv_anim_start(&anim);
  }

  AnimationState GetAnimationState() const {
    return animation_state_;
  }

 private:
  lv_obj_t* mower_ = nullptr;
  lv_obj_t* wheel_ = nullptr;

  lv_anim_t* mower_anim_ = nullptr;
  lv_anim_t* wheel_anim_ = nullptr;

  WidgetTextBar* status_bar_ = nullptr;

  AnimationState animation_state_ = AnimationState::NONE;
};

}  // namespace xbot::driver::ui::lvgl::sabo

#endif  // LVGL_SABO_SCREEN_BOOT_HPP_
