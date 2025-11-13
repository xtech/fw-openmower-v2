/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sabo_screen_sensors.hpp
 * @brief Sensor Debug/Test Screen with sensors visualization
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-11-13
 */

#ifndef LVGL_SABO_SCREEN_SENSORS_HPP_
#define LVGL_SABO_SCREEN_SENSORS_HPP_

#include <lvgl.h>
#include <ulog.h>

#include "../../SaboCoverUI/sabo_cover_ui_controller.hpp"
#include "../../SaboCoverUI/sabo_cover_ui_defs.hpp"
#include "../screen_base.hpp"
#include "../widget_sensor.hpp"
#include "globals.hpp"
#include "sabo_defs.hpp"

extern "C" {
LV_IMG_DECLARE(SaboTopView);
}

namespace xbot::driver::ui::lvgl::sabo {

class SaboScreenSensors : public ScreenBase<ScreenId, xbot::driver::ui::sabo::ButtonID> {
 public:
  SaboScreenSensors() : ScreenBase<ScreenId, xbot::driver::ui::sabo::ButtonID>(sabo::ScreenId::SENSORS) {
  }

  ~SaboScreenSensors() {
  }

  void Create(lv_color_t bg_color = lv_color_white()) override {
    ScreenBase::Create(bg_color);

    // Sabo Top View Image - fill entire screen
    lv_obj_t* image = lv_img_create(screen_);
    lv_img_set_src(image, &SaboTopView);
    lv_obj_align(image, LV_ALIGN_CENTER, 0, 0);

    // Sensor front left (centered positioning with offsets)
    sensor_front_left_ = new WidgetSensor(screen_, 18, 10, LV_ALIGN_CENTER, -69, 52);
    sensor_front_left_->SetState(WidgetSensor::State::TRIGGERED);

    // Sensor front right (centered positioning with offsets)
    sensor_front_right_ = new WidgetSensor(screen_, 18, 10, LV_ALIGN_CENTER, -69, -54);
    sensor_front_right_->SetState(WidgetSensor::State::TRIGGERED);

    // Sensor stop (centered positioning with offsets)
    sensor_stop_ = new WidgetSensor(screen_, 20, 15, LV_ALIGN_CENTER, 11, 0);
    sensor_stop_->SetState(WidgetSensor::State::TRIGGERED);

    // Sensor handle (centered positioning with offsets, different size)
    sensor_handle_ = new WidgetSensor(screen_, 12, 50, LV_ALIGN_CENTER, 94, 0);
    sensor_handle_->SetState(WidgetSensor::State::TRIGGERED);
  }

 private:
  lv_obj_t* content_container_ = nullptr;
  WidgetSensor* sensor_front_left_ = nullptr;
  WidgetSensor* sensor_front_right_ = nullptr;
  WidgetSensor* sensor_stop_ = nullptr;
  WidgetSensor* sensor_handle_ = nullptr;
};

}  // namespace xbot::driver::ui::lvgl::sabo

#endif  // LVGL_SABO_SCREEN_SENSORS_HPP_
