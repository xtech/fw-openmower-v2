/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sabo_screen_boot.cpp
 * @brief Implementation of Sabo boot screen
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-11-09
 */

#include "sabo_screen_boot.hpp"

#include <etl/string.h>

#include "../../../../robots/include/sabo_robot.hpp"
#include "chprintf.h"
#include "services.hpp"

extern "C" {
LV_IMG_DECLARE(chassis_193x94x1);
LV_IMG_DECLARE(wheel_83x83x1);
LV_FONT_DECLARE(orbitron_16b);
LV_FONT_DECLARE(orbitron_12);
}

namespace xbot::driver::ui::lvgl::sabo {

using namespace xbot::driver::sabo::types;

SaboScreenBoot::SaboScreenBoot() : ScreenBase<ScreenId, ButtonId>(ScreenId::BOOT) {
  // Initialize boot steps with hardware tests
  boot_steps_ = {{
      {"Motion Sensor", []() -> bool { return imu_service.IsFound(); }},
      {"Charger", []() -> bool { return static_cast<SaboRobot*>(robot)->TestCharger(); }},
      {"Left ESC", []() -> bool { return static_cast<SaboRobot*>(robot)->TestLeftESC(); }},
      {"Right ESC", []() -> bool { return static_cast<SaboRobot*>(robot)->TestRightESC(); }},
      {"Mower ESC", []() -> bool { return static_cast<SaboRobot*>(robot)->TestMowerESC(); }},
  }};
  current_boot_step_ = 0;
  boot_step_retry_count_ = 0;
}

SaboScreenBoot::~SaboScreenBoot() {
  // Stop and clean up animations
  if (mower_anim_) {
    lv_anim_delete(this, nullptr);  // Delete all animations for this object
    mower_anim_ = nullptr;
  }
  if (wheel_anim_) {
    lv_anim_delete(wheel_, nullptr);  // Delete all animations for wheel
    wheel_anim_ = nullptr;
  }

  // Delete status bar widget
  if (status_bar_) {
    delete status_bar_;
    status_bar_ = nullptr;
  }
}

void SaboScreenBoot::Create(lv_color_t bg_color, lv_color_t fg_color) {
  ScreenBase::Create(bg_color, fg_color);

  // Title
  lv_obj_t* title_label = lv_label_create(screen_);
  lv_label_set_text(title_label, "OpenMower @ SABO");
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
  status_bar_ = new WidgetTextBar(screen_, "Initializing ...", LV_ALIGN_BOTTOM_MID, 0, 0, lv_obj_get_width(screen_), 20,
                                  &orbitron_12);
}

void SaboScreenBoot::StartForwardAnimation() {
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
    if (self->wheel_anim_) {
      lv_anim_delete(self->wheel_anim_, nullptr);
      self->wheel_anim_ = nullptr;
    }
  });
  lv_anim_set_deleted_cb(&anim, [](lv_anim_t* a) {
    auto* self = static_cast<SaboScreenBoot*>(a->var);
    self->animation_state_ = AnimationState::DONE;
    self->mower_anim_ = nullptr;  // Clear reference when deleted
  });

  StartWheelAnimation();               // Let's rotate the wheel
  mower_anim_ = lv_anim_start(&anim);  // Start moving
}

void SaboScreenBoot::StartWheelAnimation() {
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
  wheel_anim_ = lv_anim_start(&anim);
}

bool SaboScreenBoot::OnButtonPress(ButtonId button_id) {
  (void)button_id;  // Parameter not used, we skip on any button press
  if (current_boot_step_ < BOOT_STEP_COUNT_) {
    BootStep& step = boot_steps_[current_boot_step_];
    if (step.state == BootStep::State::ERROR) {
      step.state = BootStep::State::DONE;
      current_boot_step_++;
      boot_step_retry_count_ = 0;
      return true;  // Button handled
    }
  }
  return false;  // Not handled, pass to default handling
}

void SaboScreenBoot::Tick() {
  // All boot steps done...
  if (current_boot_step_ >= BOOT_STEP_COUNT_) {
    if (animation_state_ == AnimationState::NONE) {
      SetBootStatusText("Ready to mow", 100);
      StartForwardAnimation();
    } else if (animation_state_ == AnimationState::DONE) {
      // Boot complete, invoke callback
      if (completion_callback_) {
        completion_callback_(completion_context_);
      }
    }
    return;
  }

  BootStep& step = boot_steps_[current_boot_step_];
  systime_t now = chVTGetSystemTimeX();
  uint32_t elapsed_ms = TIME_I2MS(now - step.last_action_time);

  switch (step.state) {
    case BootStep::State::WAIT:
      step.state = BootStep::State::RUNNING;
      step.last_action_time = now;
      SetBootStatusText(step.name, (current_boot_step_ * 100) / BOOT_STEP_COUNT_);
      break;

    case BootStep::State::RUNNING:
      if (elapsed_ms >= 1000) {
        if (step.test_func()) {
          step.state = BootStep::State::DONE;
          current_boot_step_++;
          boot_step_retry_count_ = 0;
        } else {
          boot_step_retry_count_++;
          if (boot_step_retry_count_ < BOOT_STEP_RETRIES_) {
            etl::string<48> fail_msg;
            chsnprintf(fail_msg.data(), fail_msg.max_size(), "%s failure %d/%d", step.name, boot_step_retry_count_,
                       BOOT_STEP_RETRIES_);
            SetBootStatusText(fail_msg, (current_boot_step_ * 100) / BOOT_STEP_COUNT_);
            step.last_action_time = now;
          } else {
            etl::string<48> fail_msg = step.name;
            fail_msg += " failed!";
            SetBootStatusText(fail_msg, (current_boot_step_ * 100) / BOOT_STEP_COUNT_);
            step.state = BootStep::State::ERROR;
            step.last_action_time = now;
          }
        }
      }
      break;

    case BootStep::State::ERROR:
      // Wait 60 seconds to continue
      if (elapsed_ms >= 60000) {
        step.state = BootStep::State::DONE;
        current_boot_step_++;
        boot_step_retry_count_ = 0;
      }
      break;

    case BootStep::State::DONE:
      // NOP. Next step gets handled in next tick
      break;
  }
}

}  // namespace xbot::driver::ui::lvgl::sabo
