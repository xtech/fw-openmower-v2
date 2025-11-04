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
 * @date 2025-6-23
 */

#ifndef OPENMOWER_SABO_COVER_UI_DISPLAY_HPP
#define OPENMOWER_SABO_COVER_UI_DISPLAY_HPP

#include <lvgl.h>
#include <ulog.h>

#include <cstring>

#include "../lvgl/sabo/sabo_defs.hpp"
#include "../lvgl/sabo/sabo_input_device_keypad.hpp"
#include "../lvgl/sabo/sabo_screen_boot.hpp"
#include "../lvgl/sabo/sabo_screen_config.hpp"
#include "../lvgl/sabo/sabo_screen_main.hpp"
#include "../lvgl/sabo/sabo_screen_menu.hpp"
#include "../lvgl/screen_base.hpp"
#include "ch.h"
#include "sabo_cover_ui_cabo_driver_base.hpp"
#include "sabo_cover_ui_defs.hpp"
#include "sabo_cover_ui_display_driver_uc1698.hpp"

namespace xbot::driver::ui {

using namespace xbot::driver::ui::sabo::display;
using namespace xbot::driver::ui::lvgl;
using namespace xbot::driver::ui::lvgl::sabo;
using DriverUC1698 = SaboCoverUIDisplayDriverUC1698;

// Type alias for Sabo screens with button handling
using SaboScreenBase = ScreenBase<ScreenId, xbot::driver::ui::sabo::ButtonID>;

class SaboCoverUIDisplay {
 public:
  explicit SaboCoverUIDisplay(LCDCfg lcd_cfg) : lcd_cfg_(lcd_cfg) {
  }

  /**
   * @brief Set the cabo driver for input device
   * @param cabo Pointer to the cabo driver
   *
   * Must be called before Init() to enable keypad input device
   */
  void SetCaboDriver(SaboCoverUICaboDriverBase* cabo) {
    cabo_ = cabo;
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

    // Initialize input device if cabo driver is set
    if (cabo_) {
      keypad_device_ = new SaboInputDeviceKeypad(cabo_);
      if (!keypad_device_->Init()) {
        ULOG_ERROR("Keypad input device initialization failed!");
        delete keypad_device_;
        keypad_device_ = nullptr;
      } else {
        // Create default group for keypad navigation
        default_group_ = lv_group_create();
        keypad_device_->SetGroup(default_group_);
        ULOG_INFO("Keypad input device initialized");
      }
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
          SaboScreenMenu::MenuItem::CONFIG,
          [](lv_event_t* e) {
            auto* display = static_cast<SaboCoverUIDisplay*>(lv_event_get_user_data(e));
            display->HideMenu();
            display->ShowConfigScreen();
          },
          this);
    }

    // Remember which screen was active before menu
    screen_before_menu_ = active_screen_;

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

  void ShowConfigScreen() {
    if (!screen_config_) {
      screen_config_ = new SaboScreenConfig();
      screen_config_->Create();

      // Set up callbacks for config changes
      screen_config_->SetOnContrastChangedCallback([](uint8_t contrast) {
        DriverUC1698::Instance().SetVBiasPotentiometer(contrast);
        ULOG_INFO("Contrast changed to %d", contrast);
      });

      screen_config_->SetOnTempCompChangedCallback([](uint8_t temp_comp) {
        DriverUC1698::Instance().SetTemperatureCompensation(temp_comp);
        ULOG_INFO("Temp compensation changed to %d", temp_comp);
      });

      screen_config_->SetOnSleepTimerChangedCallback([](uint8_t minutes) {
        // TODO: Implement auto-sleep timer
        ULOG_INFO("Auto-sleep timer changed to %d min", minutes);
      });

      screen_config_->SetOnSaveCallback([](const SaboScreenConfig::Config& config) {
        // TODO: Save config to persistent storage
        ULOG_INFO("Config saved: contrast=%d, temp_comp=%d, sleep=%d", config.contrast, config.temp_compensation,
                  config.auto_sleep_minutes);
      });
    }

    active_screen_ = screen_config_;
    screen_config_->Show();

    // Add config screen controls to group
    if (default_group_) {
      lv_group_remove_all_objs(default_group_);
      screen_config_->AddToGroup(default_group_);
    }
  }

  void HideConfigScreen() {
    if (screen_config_) {
      screen_config_->OnBackButton();  // Trigger save

      // Return to screen that was active before menu
      if (screen_before_menu_) {
        active_screen_ = screen_before_menu_;
        screen_before_menu_->Show();
      } else {
        ShowMainScreen();
      }

      // Clear group
      if (default_group_) {
        lv_group_remove_all_objs(default_group_);
      }
    }
  }

  void Tick() {
    // Backlight timeout
    if (palReadLine(lcd_cfg_.pins.backlight) == PAL_HIGH &&
        chVTTimeElapsedSinceX(backlight_last_activity_) > BACKLIGHT_TIMEOUT) {
      palWriteLine(lcd_cfg_.pins.backlight, PAL_LOW);
    }

    // Call active screen's Tick method for screen-specific updates
    if (active_screen_) {
      active_screen_->Tick();
    }

    // LCD & LVGL Timeout
    if (lvgl_display_ && DriverUC1698::Instance().IsDisplayEnabled()) {
      if (chVTTimeElapsedSinceX(lcd_last_activity_) > LCD_SLEEP_TIMEOUT) {
        DriverUC1698::Instance().SetDisplayEnable(false);
      } else {
        // One time Test-Pattern
        static bool test_pattern_drawn = false;
        if (!test_pattern_drawn) {
          test_pattern_drawn = true;
          // create_bouncing_label();
          // create_bouncing_image();
          //  lv_anim_refr_now();
        }
      }
      lv_timer_handler();  // Triggers LVGLFlushCB (if LVGL display content changed)
    }
  }

  void WakeUp() {
    // Enable Display
    if (!DriverUC1698::Instance().IsDisplayEnabled()) {
      DriverUC1698::Instance().SetDisplayEnable(true);
    }
    lcd_last_activity_ = chVTGetSystemTimeX();

    // Backlight on
    palWriteLine(lcd_cfg_.pins.backlight, PAL_HIGH);
    backlight_last_activity_ = chVTGetSystemTimeX();
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
    return active_screen_->OnButtonPress(button_id);
  }

  SaboScreenBase* GetActiveScreen() const {
    return active_screen_;
  }

  SaboScreenBoot* GetBootScreen() const {
    return screen_boot_;
  }

  SaboScreenMain* GetMainScreen() const {
    return screen_main_;
  }

  SaboScreenConfig* GetConfigScreen() const {
    return screen_config_;
  }

  SaboScreenMenu* GetMenuScreen() const {
    return screen_menu_;
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

  /**
   * @brief Get the default LVGL group for keypad navigation
   * @return Pointer to the default group, or nullptr if not initialized
   */
  lv_group_t* GetDefaultGroup() const {
    return default_group_;
  }

  /**
   * @brief Get the keypad input device
   * @return Pointer to the keypad device, or nullptr if not initialized
   */
  SaboInputDeviceKeypad* GetKeypadDevice() const {
    return keypad_device_;
  }

 private:
  LCDCfg lcd_cfg_;
  lv_display_t* lvgl_display_ = nullptr;  // LVGL display instance

  systime_t backlight_last_activity_ = 0;
  systime_t lcd_last_activity_ = 0;

  SaboScreenBoot* screen_boot_ = nullptr;
  SaboScreenMain* screen_main_ = nullptr;
  SaboScreenConfig* screen_config_ = nullptr;
  SaboScreenMenu* screen_menu_ = nullptr;
  SaboScreenBase* active_screen_ = nullptr;
  SaboScreenBase* screen_before_menu_ = nullptr;  // Remember screen before menu opened

  // Input device
  SaboCoverUICaboDriverBase* cabo_ = nullptr;
  SaboInputDeviceKeypad* keypad_device_ = nullptr;
  lv_group_t* default_group_ = nullptr;  // Default group for keypad navigation
};
}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_DISPLAY_HPP
