/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sabo_screen_inputs.hpp
 * @brief Inputs Debug/Test Screen with sensors and buttons visualization
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-11-13
 */

#ifndef LVGL_SABO_SCREEN_INPUTS_HPP_
#define LVGL_SABO_SCREEN_INPUTS_HPP_

#include <lvgl.h>
#include <ulog.h>

#include <drivers/input/sabo_input_driver.hpp>
#include <robots/include/sabo_common.hpp>

#include "../../SaboCoverUI/sabo_cover_ui_defs.hpp"
#include "../screen_base.hpp"
#include "../widget_sensor.hpp"
#include "globals.hpp"
#include "robots/include/sabo_robot.hpp"
#include "services.hpp"

extern "C" {
LV_IMG_DECLARE(SaboTopViewPortrait);
LV_FONT_DECLARE(orbitron_12);
}

using namespace xbot::driver::input;
using namespace xbot::driver::ui::sabo;
using namespace xbot::driver::sabo::defs;
using namespace xbot::driver::sabo::types;

namespace xbot::driver::ui::lvgl::sabo {

class SaboScreenInputs : public ScreenBase<ScreenId, ButtonId> {
 public:
  SaboScreenInputs() : ScreenBase<ScreenId, ButtonId>(ScreenId::INPUTS) {
  }

  ~SaboScreenInputs() {
    if (robot) {
      auto* sabo_robot = static_cast<SaboRobot*>(robot);
      sabo_robot->SetBlockButtons(false);
    }
  }

  void Create(lv_color_t bg_color = lv_color_white()) override {
    ScreenBase::Create(bg_color);

    // Sabo Top View Image - fill entire screen
    lv_obj_t* image = lv_img_create(screen_);
    lv_img_set_src(image, &SaboTopViewPortrait);
    lv_obj_align(image, LV_ALIGN_RIGHT_MID, 0, 0);

    // Create sensor widgets and store in mapping array
    // LIFT_FL
    sensors_[0].widget = new WidgetSensor(screen_, 7, 15, LV_ALIGN_CENTER, 16, -59);
    sensors_[0].widget->SetState(WidgetSensor::State::IDLE);
    // LIFT_FR
    sensors_[1].widget = new WidgetSensor(screen_, 7, 15, LV_ALIGN_CENTER, 96, -59);
    sensors_[1].widget->SetState(WidgetSensor::State::IDLE);
    // STOP_TOP
    sensors_[2].widget = new WidgetSensor(screen_, 10, 15, LV_ALIGN_CENTER, 56, 8);
    sensors_[2].widget->SetState(WidgetSensor::State::IDLE);
    // STOP_REAR
    sensors_[3].widget = new WidgetSensor(screen_, 38, 8, LV_ALIGN_CENTER, 56, 75);
    sensors_[3].widget->SetState(WidgetSensor::State::IDLE);

    // Create Button test label in upper-left corner
    lv_obj_t* btn_label_ = lv_label_create(screen_);
    lv_label_set_text_static(btn_label_, "Button Test");
    lv_obj_set_style_text_color(btn_label_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(btn_label_, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(btn_label_, LV_ALIGN_TOP_MID, -(LCD_WIDTH / 4), 0);
    // ... and value
    btn_test_value_ = lv_label_create(screen_);
    lv_label_set_text_static(btn_test_value_, "?");
    lv_obj_set_style_text_color(btn_test_value_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(btn_test_value_, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(btn_test_value_, LV_ALIGN_TOP_MID, -(LCD_WIDTH / 4), 15);

    // Create heartbeat label in lower-left corner
    lv_obj_t* heartbeat_label_ = lv_label_create(screen_);
    lv_label_set_text_static(heartbeat_label_, "Rear Handle\n  Heartbeat");
    lv_obj_set_style_text_color(heartbeat_label_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(heartbeat_label_, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(heartbeat_label_, LV_ALIGN_BOTTOM_MID, -(LCD_WIDTH / 4), -15);

    // ... and value
    heartbeat_value_ = lv_label_create(screen_);
    lv_label_set_text_static(heartbeat_value_, "0 Hz");
    lv_obj_set_style_text_color(heartbeat_value_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(heartbeat_value_, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(heartbeat_value_, LV_ALIGN_BOTTOM_MID, -(LCD_WIDTH / 4), 0);
  }

  void Show() override {
    // Enable button blocking when screen is shown
    if (robot) {
      auto* sabo_robot = static_cast<SaboRobot*>(robot);
      sabo_robot->SetBlockButtons(true);
    }
    ScreenBase::Show();
  }

  void Hide() override {
    // Disable button blocking when screen is hidden
    if (robot) {
      auto* sabo_robot = static_cast<SaboRobot*>(robot);
      sabo_robot->SetBlockButtons(false);
    }
    ScreenBase::Hide();
  }

  void Tick() override {
    // Direct polling: update sensor display from GPIO states
    if (robot) {
      auto* sabo_robot = static_cast<SaboRobot*>(robot);
      for (auto& sensor : sensors_) {
        if (sensor.widget) {
          bool active = sabo_robot->GetSensorState(sensor.sensor_id);
          sensor.widget->SetState(active ? WidgetSensor::State::TRIGGERED : WidgetSensor::State::IDLE);
        }
      }

      // Update heartbeat frequency display
      if (heartbeat_value_) {
        uint16_t heartbeat_frequency = sabo_robot->GetSensorHeartbeatFrequency();
        static char heartbeat_text[20];
        snprintf(heartbeat_text, sizeof(heartbeat_text), "%u Hz", heartbeat_frequency);
        lv_label_set_text(heartbeat_value_, heartbeat_text);
      }

      // Update button test display
      if (btn_test_value_) {
        const char* pressed_button = "?";  // Default for no button pressed

        // Loop through ALL_BUTTONS array and check each button
        for (auto button_id : ALL_BUTTONS) {
          if (sabo_robot->IsButtonPressed(button_id)) {
            pressed_button = ButtonIdToString(button_id);
            break;  // Show only the first pressed button
          }
        }

        lv_label_set_text_static(btn_test_value_, pressed_button);
      }
    }
  }

 private:
  struct SensorWidget {
    SensorId sensor_id;
    WidgetSensor* widget = nullptr;
  };

  // Sensor mapping: index matches SensorId enum values (0-3)
  SensorWidget sensors_[4] = {
      {SensorId::LIFT_FL, nullptr},
      {SensorId::LIFT_FR, nullptr},
      {SensorId::STOP_TOP, nullptr},
      {SensorId::STOP_REAR, nullptr},
  };

  event_listener_t event_listener_{};

  lv_obj_t* content_container_ = nullptr;
  WidgetSensor* sensor_front_left_ = nullptr;
  WidgetSensor* sensor_front_right_ = nullptr;
  WidgetSensor* sensor_stop_ = nullptr;
  WidgetSensor* sensor_handle_ = nullptr;
  lv_obj_t* heartbeat_value_ = nullptr;
  lv_obj_t* btn_test_value_ = nullptr;

  /**
   * @brief Update sensor display based on current input states
   * Called when INPUTS_CHANGED event is received
   */
  void UpdateSensorDisplay() {
    // Update all sensors based on InputService state
    /*uint64_t active_mask = input_service.GetActiveInputsMask();

    // Sensor 0: Front left wheel lift
    if (sensor_front_left_) {
      bool active = (active_mask >> 0) & 1;
      sensor_front_left_->SetState(active ? WidgetSensor::State::TRIGGERED : WidgetSensor::State::IDLE);
    }

    // Sensor 1: Front right wheel lift
    if (sensor_front_right_) {
      bool active = (active_mask >> 1) & 1;
      sensor_front_right_->SetState(active ? WidgetSensor::State::TRIGGERED : WidgetSensor::State::IDLE);
    }

    // Sensor 2: Top stop button
    if (sensor_stop_) {
      bool active = (active_mask >> 2) & 1;
      sensor_stop_->SetState(active ? WidgetSensor::State::TRIGGERED : WidgetSensor::State::IDLE);
    }

    // Sensor 3: Back-handle stop
    if (sensor_handle_) {
      bool active = (active_mask >> 3) & 1;
      sensor_handle_->SetState(active ? WidgetSensor::State::TRIGGERED : WidgetSensor::State::IDLE);
    }*/
  }
};

}  // namespace xbot::driver::ui::lvgl::sabo

#endif  // LVGL_SABO_SCREEN_INPUTS_HPP_
