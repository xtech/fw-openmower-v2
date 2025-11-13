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

#include "../lvgl/sabo/sabo_defs.hpp"
#include "../lvgl/sabo/sabo_input_device_keypad.hpp"
#include "../lvgl/sabo/sabo_menu_main.hpp"
#include "../lvgl/sabo/sabo_screen_about.hpp"
#include "../lvgl/sabo/sabo_screen_boot.hpp"
#include "../lvgl/sabo/sabo_screen_main.hpp"
#include "../lvgl/sabo/sabo_screen_settings.hpp"
#include "../lvgl/screen_base.hpp"
#include "ch.h"
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
  explicit SaboCoverUIDisplay(LCDCfg lcd_cfg, const ButtonCheckCallback& button_check_callback = ButtonCheckCallback())
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
      return true;  // Continue anyway - there might be CoverUI boards which don't have an LCD
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

    // Initialize input device
    if (!keypad_device_.Init()) {
      ULOG_ERROR("Keypad input device initialization failed!");
      // Keypad device remains initialized but won't work properly
    } else {
      input_group_ = lv_group_create();
      keypad_device_.SetGroup(input_group_);
    }

    return true;
  };

  void Start() {
    DriverUC1698::Instance().Start();

    // Disable boot screen here for faster testing
    ShowBootScreen();
    // ShowMainScreen();
    //  ShowAboutScreen();
  }

  void ShowBootScreen() {
    if (!screen_boot_) {
      screen_boot_ = new SaboScreenBoot();
      screen_boot_->Create();

      // Set completion callback to transition to main screen
      screen_boot_->SetCompletionCallback(OnBootComplete, this);
    }
    active_screen_ = screen_boot_;
    screen_boot_->Show();
  }

  void ShowMainScreen() {
    GetOrCreateScreen(screen_main_);
    active_screen_ = screen_main_;
    screen_main_->Show();
  }

  void ShowSettingsScreen() {
    // If menu is open, hide it immediately (without animation) before switching screens
    SafeDelete(menu_main_);

    if (active_screen_) {
      active_screen_->Deactivate();
      active_screen_->Hide();
    }

    GetOrCreateScreen(screen_settings_);
    active_screen_ = screen_settings_;
    screen_settings_->Show();
    screen_settings_->Activate(input_group_);
  }

  void CloseSettingsScreen() {
    // Clean up menu if it's open (it might be attached to Settings screen)
    SafeDelete(menu_main_);

    if (screen_settings_) {
      lcd_settings_ = screen_settings_->GetSettings();
      // Switch to main screen BEFORE deleting settings screen to avoid deleting the active LVGL screen
      ShowMainScreen();
      SafeDelete(screen_settings_);
    }

    // Clear group
    if (input_group_) {
      lv_group_remove_all_objs(input_group_);
    }
  }

  void ShowAboutScreen() {
    // If menu is open, hide it immediately (without animation) before switching screens
    SafeDelete(menu_main_);

    if (active_screen_) {
      active_screen_->Deactivate();
      active_screen_->Hide();
    }

    GetOrCreateScreen(screen_about_);
    active_screen_ = screen_about_;
    screen_about_->Show();
    screen_about_->Activate(input_group_);
  }

  void CloseAboutScreen() {
    // Clean up menu if it's open (it might be attached to About screen)
    SafeDelete(menu_main_);

    if (screen_about_) {
      // Switch to main screen BEFORE deleting about screen to avoid deleting the active LVGL screen
      ShowMainScreen();
      SafeDelete(screen_about_);
    }

    // Clear group
    if (input_group_) {
      lv_group_remove_all_objs(input_group_);
    }
  }

  void ShowMenu() {
    // Don't create a new menu if one is still animating out
    if (menu_main_ && menu_main_->GetAnimationState() == SaboMenuMain::AnimationState::SLIDING_OUT) {
      ULOG_DEBUG("Menu still sliding out, ignoring ShowMenu()");
      return;
    }

    // Delete old menu if exists (cleanup from previous hide)
    SafeDelete(menu_main_);

    menu_main_ = new SaboMenuMain();

    // Set callback to delete menu after it closes
    menu_main_->SetClosedCallback(OnMenuClosed, this);

    // Create menu as overlay on the ACTIVE screen
    if (active_screen_ && active_screen_->GetLvScreen()) {
      menu_main_->CreateOverlay(active_screen_->GetLvScreen());
    } else {
      ULOG_ERROR("No active screen for menu!");
      return;
    }

    // Set up menu item callbacks
    menu_main_->SetMenuItemCallback(
        SaboMenuMain::MenuItem::SETTINGS,
        [](lv_event_t* e) {
          auto* display = static_cast<SaboCoverUIDisplay*>(lv_event_get_user_data(e));
          // Don't call HideMenu() here - ShowSettingsScreen() will handle menu cleanup
          display->ShowSettingsScreen();
        },
        this);
    menu_main_->SetMenuItemCallback(
        SaboMenuMain::MenuItem::ABOUT,
        [](lv_event_t* e) {
          auto* display = static_cast<SaboCoverUIDisplay*>(lv_event_get_user_data(e));
          // Don't call HideMenu() here - ShowAboutScreen() will handle menu cleanup
          display->ShowAboutScreen();
        },
        this);

    // Clear group to prevent underlying screen from receiving input
    if (input_group_) {
      lv_group_remove_all_objs(input_group_);
      // Add only menu items to group
      menu_main_->AddToGroup(input_group_);
    }

    menu_main_->SlideIn();
  }

  void HideMenu() {
    if (menu_main_) {
      menu_main_->SlideOut();
      // Menu will be deleted in OnMenuClosed callback after animation completes

      // Clear group and restore underlying screen's focus
      if (input_group_) {
        lv_group_remove_all_objs(input_group_);

        // Restore focus to active screen if it has focusable items
        if (active_screen_) {
          active_screen_->Activate(input_group_);
        }
      }
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

    // If main menu isn't open, let the active screen handle the button first
    if (!menu_main_ && active_screen_->OnButtonPress(button_id)) {
      return true;  // Handled by screen
    }

    // If screen didn't handle it, global button logic here
    switch (button_id) {
      case ButtonID::MENU:
        // Allow menu on all screens except BOOT
        if (active_screen_->GetScreenId() != ScreenId::BOOT) {
          // Toggle menu: if open, close it; if closed, open it
          if (menu_main_ && (menu_main_->GetAnimationState() == SaboMenuMain::AnimationState::VISIBLE ||
                             menu_main_->GetAnimationState() == SaboMenuMain::AnimationState::SLIDING_IN)) {
            // Closing menu - save settings if we're on settings screen
            if (active_screen_->GetScreenId() == ScreenId::SETTINGS && screen_settings_) {
              screen_settings_->SaveSettings();
            }
            HideMenu();
          } else {
            ShowMenu();
          }
          return true;  // Handled
        }
        break;
      case ButtonID::BACK:
        if (active_screen_->GetScreenId() == ScreenId::SETTINGS) {
          if (screen_settings_) {
            screen_settings_->SaveSettings();
          }
          CloseSettingsScreen();
          return true;  // Handled
        } else if (active_screen_->GetScreenId() == ScreenId::ABOUT) {
          CloseAboutScreen();
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

 private:
  LCDCfg lcd_cfg_;
  lv_display_t* lvgl_display_ = nullptr;  // LVGL display instance
  settings::LCDSettings lcd_settings_;    // LCD settings for timeout calculations

  systime_t last_activity_ = 0;

  SaboScreenBoot* screen_boot_ = nullptr;
  SaboScreenMain* screen_main_ = nullptr;
  SaboScreenAbout* screen_about_ = nullptr;
  SaboScreenSettings* screen_settings_ = nullptr;
  SaboMenuMain* menu_main_ = nullptr;
  SaboScreenBase* active_screen_ = nullptr;

  const ButtonCheckCallback& button_check_callback_;  // Button check callback delegate (const ref)

  // Input device (owned by display)
  SaboInputDeviceKeypad keypad_device_{button_check_callback_};  // Static allocation
  lv_group_t* input_group_ = nullptr;

  /**
   * @brief Helper template to safely delete and nullify a pointer
   * @tparam T The type of the pointer
   * @param ptr Reference to the pointer to delete
   */
  template <typename T>
  void SafeDelete(T*& ptr) {
    if (ptr) {
      delete ptr;
      ptr = nullptr;
    }
  }

  /**
   * @brief Helper template to create a screen if it doesn't exist
   * @tparam ScreenT The screen type (e.g., SaboScreenMain, SaboScreenSettings, ...)
   * @param screen_ptr Reference to the screen pointer member variable
   */
  template <typename ScreenT>
  void GetOrCreateScreen(ScreenT*& screen_ptr) {
    if (!screen_ptr) {
      screen_ptr = new ScreenT();
      screen_ptr->Create();
    }
  }

  // Static callback for boot completion
  static void OnBootComplete(void* context) {
    if (!context) return;
    auto* display = static_cast<SaboCoverUIDisplay*>(context);

    display->SafeDelete(display->screen_boot_);
    display->ShowMainScreen();
  }

  // Static callback for menu closed
  static void OnMenuClosed(void* context) {
    if (!context) return;
    auto* display = static_cast<SaboCoverUIDisplay*>(context);

    display->SafeDelete(display->menu_main_);
  }
};
}  // namespace xbot::driver::ui

#endif  // OPENMOWER_SABO_COVER_UI_DISPLAY_HPP
