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

#include <cstring>

#include "../lvgl/sabo/sabo_defs.hpp"
#include "../lvgl/sabo/sabo_input_device_keypad.hpp"
#include "../lvgl/sabo/sabo_screen_boot.hpp"
#include "../lvgl/sabo/sabo_screen_main.hpp"
#include "../lvgl/sabo/sabo_screen_menu.hpp"
#include "../lvgl/sabo/sabo_screen_settings.hpp"
#include "../lvgl/screen_base.hpp"
#include "ch.h"
#include "sabo_cover_ui_cabo_driver_base.hpp"
#include "sabo_cover_ui_defs.hpp"
#include "sabo_cover_ui_display_driver_uc1698.hpp"

namespace xbot::driver::ui {

using namespace xbot::driver::ui::sabo;
using namespace xbot::driver::ui::lvgl;
using namespace xbot::driver::ui::lvgl::sabo;

using DriverUC1698 = SaboCoverUIDisplayDriverUC1698;
using SaboScreenBase = ScreenBase<ScreenId, ButtonID>;
using ButtonCheckCallback = etl::delegate<bool(ButtonID)>;

class SaboCoverUIDisplay {
 public:
  explicit SaboCoverUIDisplay(LCDCfg lcd_cfg, ButtonCheckCallback button_check_callback = ButtonCheckCallback())
      : lcd_cfg_(lcd_cfg), button_check_callback_(button_check_callback) {
    // Load LCD settings
    if (!settings::LCDSettings::Load(lcd_settings_)) {
      ULOG_DEBUG("LCD settings file not found, using defaults");
    }
  }

  bool Init() {
    // Backlight
    if (lcd_cfg_.pins.backlight != PAL_NOLINE) {
      palSetLineMode(lcd_cfg_.pins.backlight, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2);
      palWriteLine(lcd_cfg_.pins.backlight, PAL_LOW);
    }

    // Initialize and start the LCD driver
    if (!DriverUC1698::Instance(&lcd_cfg_).Init()) {
      ULOG_ERROR("SaboCoverUIDisplayDriverUC1698 initialization failed!");
      return true;  // We shouldn't fail here for those without a connected display
    }

    // Init LVGL, CBs, buffer, display driver, ...
    lv_init();

    // Redirect LVGL log output to ULOG
#if LV_USE_LOG
    lv_log_register_print_cb([](lv_log_level_t level, const char* buf) {
      (void)level;
      ULOG_INFO("[LVGL] %s", buf);
    });
#endif

    lv_tick_set_cb([]() -> uint32_t { return TIME_I2MS(chVTGetSystemTimeX()); });

    // 1/BUFFER_FRACTION screen buffer / 8 pixel per byte, + 2*4 byte palette
    static uint8_t draw_buf1[(LCD_WIDTH * LCD_HEIGHT / BUFFER_FRACTION / 8) + 8];
    lvgl_display_ = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    lv_display_add_event_cb(lvgl_display_, DriverUC1698::LVGLRounderCB, LV_EVENT_INVALIDATE_AREA, NULL);
    lv_display_set_flush_cb(lvgl_display_, DriverUC1698::LVGLFlushCB);
    lv_display_set_flush_wait_cb(lvgl_display_, DriverUC1698::LVGLFlushWaitCB);
    lv_display_set_buffers(lvgl_display_, draw_buf1, NULL, sizeof(draw_buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Initialize input device (AFTER LVGL is initialized!)
    if (!keypad_device_.Init()) {
      ULOG_ERROR("Keypad input device initialization failed!");
      // Keypad device remains initialized but won't work properly
    } else {
      default_group_ = lv_group_create();
      keypad_device_.SetGroup(default_group_);
    }

    return true;
  };

  void Start() {
    DriverUC1698::Instance().Start();

    // ShowBootScreen();
    ShowMainScreen();
  }

  void SetBootStatus(const etl::string_view& text, int progress) {
    if (screen_boot_) screen_boot_->SetBootStatus(text, progress);
  }

  void ShowBootScreen() {
    if (!screen_boot_) {
      screen_boot_ = new SaboScreenBoot();
      screen_boot_->Create();
    }
    active_screen_ = screen_boot_;
    screen_boot_->Show();
  }

  void ShowMainScreen() {
    if (!screen_main_) {
      screen_main_ = new SaboScreenMain();
      screen_main_->Create();
    }
    active_screen_ = screen_main_;
    screen_main_->Show();
    if (screen_boot_) {
      delete screen_boot_;
      screen_boot_ = nullptr;
    }
  }

  void ShowMenu() {
    if (!screen_menu_) {
      screen_menu_ = new SaboScreenMenu();
      // Create menu as overlay on the ACTIVE screen
      if (active_screen_ && active_screen_->GetLvScreen()) {
        screen_menu_->CreateOverlay(active_screen_->GetLvScreen());
      } else {
        ULOG_ERROR("No active screen for menu!");
        return;
      }

      // Set up menu item callbacks
      screen_menu_->SetMenuItemCallback(
          SaboScreenMenu::MenuItem::SETTINGS,
          [](lv_event_t* e) {
            auto* display = static_cast<SaboCoverUIDisplay*>(lv_event_get_user_data(e));
            display->HideMenu();
            display->ShowSettingsScreen();
          },
          this);
    }

    // Clear group to prevent underlying screen from receiving input
    if (default_group_) {
      lv_group_remove_all_objs(default_group_);
      // Add only menu items to group
      screen_menu_->AddToGroup(default_group_);
    }

    screen_menu_->SlideIn();
  }

  void HideMenu() {
    if (screen_menu_) {
      screen_menu_->SlideOut();

      // Clear group and restore underlying screen's focus
      if (default_group_) {
        lv_group_remove_all_objs(default_group_);
        // TODO: If active screen has focusable items, add them back here
        // For now, main screen has no focusable items, so we just clear
      }
    }
  }

  void ShowSettingsScreen() {
    if (active_screen_) {
      active_screen_->Deactivate();
      active_screen_->Hide();
      // TODO: Delete?!
    }

    if (!screen_settings_) {
      screen_settings_ = new SaboScreenSettings();
      screen_settings_->Create();
    }

    active_screen_ = screen_settings_;
    screen_settings_->Show();
    screen_settings_->Activate(default_group_);
  }

  void CloseSettingsScreen() {
    if (screen_settings_) {
      lcd_settings_ = screen_settings_->GetSettings();
      delete screen_settings_;
      screen_settings_ = nullptr;
      ShowMainScreen();
    }

    // Clear group
    if (default_group_) {
      lv_group_remove_all_objs(default_group_);
    }
  }

  void Tick() {
    // Calculate timeout from settings (convert minutes to system time)
    const systime_t timeout = TIME_S2I(lcd_settings_.auto_sleep_minutes * 60);

    // Call active screen's Tick method for screen-specific updates
    if (active_screen_) {
      active_screen_->Tick();
    }

    // Backlight & LCD Timeout
    if (chVTTimeElapsedSinceX(last_activity_) > timeout) {
      // Backlight off
      if (palReadLine(lcd_cfg_.pins.backlight) == PAL_HIGH) {
        palWriteLine(lcd_cfg_.pins.backlight, PAL_LOW);
      }
      // LCD off
      if (lvgl_display_ && DriverUC1698::Instance().IsDisplayEnabled()) {
        DriverUC1698::Instance().SetDisplayEnable(false);
      }
    }

    lv_timer_handler();
  }

  void WakeUp() {
    // Enable Display
    if (!DriverUC1698::Instance().IsDisplayEnabled()) {
      DriverUC1698::Instance().SetDisplayEnable(true);
    }

    // Backlight on
    palWriteLine(lcd_cfg_.pins.backlight, PAL_HIGH);
    last_activity_ = chVTGetSystemTimeX();
  }

  /**
   * @brief Handle button press by delegating to active screen
   * @param button_id The button that was pressed
   * @return true if button was handled, false otherwise
   *
   * This method allows screens to implement their own button logic.
   * If a screen doesn't handle a button (returns false), the caller
   * can implement global button handling (e.g., MENU button behavior).
   */
  bool OnButtonPress(xbot::driver::ui::sabo::ButtonID button_id) {
    if (!active_screen_) return false;

    // Let the active screen handle the button first
    if (active_screen_->OnButtonPress(button_id)) {
      return true;  // Handled by screen
    }

    // If screen didn't handle it, global button logic here
    switch (button_id) {
      case ButtonID::MENU:
        if (active_screen_->GetScreenId() == ScreenId::MAIN) {
          ShowMenu();
          return true;  // Handled
        }
        break;
      case ButtonID::BACK:
        if (active_screen_->GetScreenId() == ScreenId::SETTINGS) {
          screen_settings_->SaveSettings();
          CloseSettingsScreen();
          return true;  // Handled
        } else if (active_screen_->GetScreenId() != ScreenId::BOOT) {
          // BACK from other screens hides menu
          HideMenu();
          return true;  // Handled
        }
        break;
      default: break;
    }
    return false;  // Not handled
  }

  SaboScreenBase* GetActiveScreen() const {
    return active_screen_;
  }

  SaboScreenBoot* GetBootScreen() const {
    return screen_boot_;
  }

  /**
   * @brief Check if menu is currently visible
   * @return true if menu is visible or sliding in/out
   */
  bool IsMenuVisible() const {
    if (!screen_menu_) return false;
    auto state = screen_menu_->GetAnimationState();
    return state == SaboScreenMenu::AnimationState::VISIBLE || state == SaboScreenMenu::AnimationState::SLIDING_IN ||
           state == SaboScreenMenu::AnimationState::SLIDING_OUT;
  }

 private:
  LCDCfg lcd_cfg_;
  lv_display_t* lvgl_display_ = nullptr;  // LVGL display instance
  settings::LCDSettings lcd_settings_;    // LCD settings for timeout calculations

  systime_t last_activity_ = 0;

  SaboScreenBoot* screen_boot_ = nullptr;
  SaboScreenMain* screen_main_ = nullptr;
  SaboScreenSettings* screen_settings_ = nullptr;
  SaboScreenMenu* screen_menu_ = nullptr;
  SaboScreenBase* active_screen_ = nullptr;
  SaboScreenBase* screen_before_menu_ = nullptr;  // Remember screen before menu opened

  ButtonCheckCallback button_check_callback_;  // Button check callback delegate

  // Input device (owned by display)
  SaboInputDeviceKeypad keypad_device_{button_check_callback_};  // Static allocation
  lv_group_t* default_group_ = nullptr;
};
}  // namespace xbot::driver::ui

#endif  // OPENMOWER_SABO_COVER_UI_DISPLAY_HPP
