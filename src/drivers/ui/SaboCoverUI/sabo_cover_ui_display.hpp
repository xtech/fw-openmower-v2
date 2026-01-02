/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sabo_cover_ui_display.hpp
 * @brief Sabo Cover UI Display management with LVGL and UC1698 driver
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-11-08
 */

#ifndef OPENMOWER_SABO_COVER_UI_DISPLAY_HPP
#define OPENMOWER_SABO_COVER_UI_DISPLAY_HPP

#include <etl/delegate.h>
#include <lvgl.h>
#include <ulog.h>

#include "../lvgl/sabo/sabo_input_device_keypad.hpp"
#include "../lvgl/sabo/sabo_menu_main.hpp"
#include "../lvgl/sabo/sabo_screen_about.hpp"
#include "../lvgl/sabo/sabo_screen_battery.hpp"
#include "../lvgl/sabo/sabo_screen_boot.hpp"
#include "../lvgl/sabo/sabo_screen_inputs.hpp"
#include "../lvgl/sabo/sabo_screen_main.hpp"
#include "../lvgl/sabo/sabo_screen_settings.hpp"
#include "../lvgl/screen_base.hpp"
#include "ch.h"
#include "robots/include/sabo_common.hpp"
#include "sabo_cover_ui_display_driver_uc1698.hpp"

namespace xbot::driver::ui {

using namespace xbot::driver::ui::lvgl;
using namespace xbot::driver::ui::lvgl::sabo;
using namespace xbot::driver::sabo::types;

using DriverUC1698 = SaboCoverUIDisplayDriverUC1698;
using SaboScreenBase = ScreenBase<ScreenId, ButtonId>;
using ButtonCheckCallback = etl::delegate<bool(ButtonId)>;

/**
 * @brief Main display controller for Sabo Cover UI
 *
 * Manages LVGL display, screens, input handling and display driver
 */
class SaboCoverUIDisplay {
 public:
  explicit SaboCoverUIDisplay(const config::Lcd* lcd_cfg,
                              const ButtonCheckCallback& button_check_callback = ButtonCheckCallback());

  bool Init();
  void Start();

  void ShowBootScreen();
  void ShowMainScreen();
  void ShowSettingsScreen();
  void CloseSettingsScreen();
  void ShowAboutScreen();
  void CloseAboutScreen();
  void ShowBatteryScreen();
  void CloseBatteryScreen();
  void ShowInputsScreen();
  void CloseInputsScreen();

  void ShowMenu();
  void HideMenu();

  void Tick();
  void WakeUp();
  bool OnButtonPress(ButtonId button_id);

 private:
  const config::Lcd* lcd_cfg_;
  lv_display_t* lvgl_display_ = nullptr;
  settings::LCDSettings lcd_settings_;

  systime_t last_activity_ = 0;

  SaboScreenBoot* screen_boot_ = nullptr;
  SaboScreenMain* screen_main_ = nullptr;
  SaboScreenAbout* screen_about_ = nullptr;
  SaboScreenSettings* screen_settings_ = nullptr;
  SaboScreenInputs* screen_inputs_ = nullptr;
  SaboScreenBattery* screen_battery_ = nullptr;
  SaboMenuMain* menu_main_ = nullptr;
  SaboScreenBase* active_screen_ = nullptr;

  const ButtonCheckCallback& button_check_callback_;

  SaboInputDeviceKeypad keypad_device_;
  lv_group_t* input_group_ = nullptr;

  template <typename T>
  void SafeDelete(T*& ptr) {
    if (ptr) {
      delete ptr;
      ptr = nullptr;
    }
  }

  template <typename ScreenT>
  void SwitchToScreen(ScreenT*& screen_ptr) {
    // If menu is open, hide it immediately (without animation) before switching screens
    SafeDelete(menu_main_);

    if (active_screen_) {
      active_screen_->Deactivate();
      active_screen_->Hide();
    }

    // Create screen if needed (inlined from CreateIfNeeded)
    if (!screen_ptr) {
      screen_ptr = new ScreenT();
      screen_ptr->Create();
    }

    active_screen_ = screen_ptr;
    screen_ptr->Show();
    screen_ptr->Activate(input_group_);
  }

  template <typename ScreenT>
  void CloseScreen(ScreenT*& screen_ptr) {
    // Clean up menu if it's open (it might be attached to the screen)
    SafeDelete(menu_main_);

    if (screen_ptr) {
      // Switch to main screen BEFORE deleting screen to avoid deleting the active LVGL screen
      ShowMainScreen();
      SafeDelete(screen_ptr);
    }

    // Clear group
    if (input_group_) {
      lv_group_remove_all_objs(input_group_);
    }
  }

  static void OnBootComplete(void* context);
  static void OnMenuClosed(void* context);
};
}  // namespace xbot::driver::ui

#endif  // OPENMOWER_SABO_COVER_UI_DISPLAY_HPP
