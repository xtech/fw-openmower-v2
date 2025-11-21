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

#include <etl/array.h>
#include <etl/string_view.h>
#include <lvgl.h>

#include "../../SaboCoverUI/sabo_cover_ui_defs.hpp"
#include "../screen_base.hpp"
#include "../widget_textbar.hpp"
#include "ch.h"
#include "globals.hpp"
#include "sabo_defs.hpp"

namespace xbot::driver::ui::lvgl::sabo {

using namespace xbot::driver::ui::sabo;

// Boot test callback function pointer type
using BootTestCallback = bool (*)();
using BootCompletionCallback = void (*)(void* context);

class SaboScreenBoot : public ScreenBase<ScreenId, ButtonID> {
 public:
  SaboScreenBoot();
  ~SaboScreenBoot();

  /**
   * @brief Set callback to invoke when boot sequence completes
   * @param callback Function pointer to call when boot is done
   * @param context User context pointer passed to callback
   */
  void SetCompletionCallback(BootCompletionCallback callback, void* context = nullptr) {
    completion_callback_ = callback;
    completion_context_ = context;
  }

  void Create(lv_color_t bg_color = lv_color_white()) override;

  /**
   * @brief Override Tick to handle boot sequence state machine
   */
  void Tick() override;

 private:
  // LVGL objects
  lv_obj_t* mower_ = nullptr;
  lv_obj_t* wheel_ = nullptr;
  lv_anim_t* mower_anim_ = nullptr;
  lv_anim_t* wheel_anim_ = nullptr;
  WidgetTextBar* status_bar_ = nullptr;

  // Animation state
  enum class AnimationState { NONE, STARTED, DONE };
  AnimationState animation_state_ = AnimationState::NONE;

  // Boot sequence related
  static constexpr size_t BOOT_STEP_COUNT_ = 5;
  static constexpr size_t BOOT_STEP_RETRIES_ = 3;

  struct BootStep {
    const char* name;
    BootTestCallback test_func;
    enum class State { WAIT, RUNNING, ERROR, DONE } state = State::WAIT;
    systime_t last_action_time = 0;
  };

  etl::array<BootStep, BOOT_STEP_COUNT_> boot_steps_;

  size_t current_boot_step_ = 0;
  size_t boot_step_retry_count_ = 0;

  BootCompletionCallback completion_callback_ = nullptr;
  void* completion_context_ = nullptr;

  void SetBootStatusText(const etl::string_view& text, int progress) {
    if (status_bar_) {
      status_bar_->SetValue(progress, text.data());
    }
  }

  // Helper methods
  void StartForwardAnimation();
  void StartWheelAnimation();
};

}  // namespace xbot::driver::ui::lvgl::sabo

#endif  // LVGL_SABO_SCREEN_BOOT_HPP_
