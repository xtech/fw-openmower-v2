#ifndef LVGL_SABO_SCREEN_BOOT_HPP_
#define LVGL_SABO_SCREEN_BOOT_HPP_

#include <lvgl.h>

#include "sabo_defs.hpp"
#include "screen_base.hpp"
#include "widget_textbar.hpp"

extern "C" {
LV_IMG_DECLARE(chassis_193x94x1);
LV_IMG_DECLARE(wheel_83x83x1);
}

namespace xbot::driver::ui::lvgl::sabo {

class SaboScreenBoot : public ScreenBase<sabo::ScreenId> {
 public:
  enum class BootStage {
    INIT,
    CHECK_HARDWARE,
    INIT_SENSORS,
    INIT_MOTORS,
    INIT_NAVIGATION,
    INIT_COMPLETE,
    ANIM_STARTED,
    ANIM_COMPLETED
  };

  SaboScreenBoot() : ScreenBase<sabo::ScreenId>(sabo::ScreenId::BOOT) {
  }

  ~SaboScreenBoot() {
    // Cleanup des Timers
    if (boot_timer_) {
      lv_timer_delete(boot_timer_);
      boot_timer_ = nullptr;
    }

    // Cleanup des dynamisch allozierten Widgets
    if (status_bar_) {
      delete status_bar_;
      status_bar_ = nullptr;
    }
  }

  void Create(lv_color_t bg_color = lv_color_white()) override {
    ScreenBase::Create(bg_color);

    // Title
    lv_obj_t *title_label = lv_label_create(screen_);
    lv_label_set_text(title_label, "OpenMower@SABO");
    lv_obj_set_style_text_color(title_label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title_label, &lv_font_unscii_16, LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 0);

    // Mower (chassis and wheel) container
    mower_ = lv_obj_create(screen_);
    lv_obj_set_size(mower_, 220, 99);  // mower + wheel size
    lv_obj_clear_flag(mower_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(mower_, 0, LV_PART_MAIN);
    lv_obj_align(mower_, LV_ALIGN_CENTER, 0, 0);

    // Chassis
    lv_obj_t *chassis = lv_img_create(mower_);
    lv_img_set_src(chassis, &chassis_193x94x1);
    lv_obj_align(chassis, LV_ALIGN_CENTER, -14, -2);

    // Wheel (static initially)
    wheel_ = lv_img_create(mower_);
    lv_img_set_src(wheel_, &wheel_83x83x1);
    lv_obj_align_to(wheel_, chassis, LV_ALIGN_CENTER, 83, 10);  // Position relativ zum Chassis
    lv_obj_set_style_transform_pivot_x(wheel_, 41, 0);          // Center of 83x83 image
    lv_obj_set_style_transform_pivot_y(wheel_, 41, 0);          // Center of 83x83 image

    // Status-Bar mit Text ganz unten über volle Bildschirmbreite
    // Warten bis Screen vollständig initialisiert ist
    lv_obj_update_layout(screen_);

    status_bar_ = new WidgetTextBar(screen_,                    // Parent Screen
                                    "Initializing ...",         // Format-String für Boot-Fortschritt
                                    LV_ALIGN_BOTTOM_MID,        // Unten zentriert
                                    0, 0,                       // X-Offset 0, Y-Offset
                                    lv_obj_get_width(screen_),  // Width
                                    20                          // Height
    );

    // Anfangswerte setzen und Widget sichtbar machen
    if (status_bar_) {
      status_bar_->SetRange(0, 100);
      status_bar_->SetValue(10);  // Startwert zum Test

      // Debug: Widget sichtbar und über anderen Objekten
      lv_obj_t *bar_obj = status_bar_->GetLvglObject();
      if (bar_obj) {
        lv_obj_move_foreground(bar_obj);
        lv_obj_clear_flag(bar_obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_invalidate(bar_obj);
      }
    }

    SimulateBootSequence();  // Start boot sequence simulation
  }

  void StartForwardAnimation() {
    if (!mower_) return;

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
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in);      // Start slow, accelerate
    lv_anim_set_exec_cb(&anim, [](void *var, int32_t v) {  // Animator callback
      auto *self = static_cast<SaboScreenBoot *>(var);
      lv_obj_align(self->mower_, LV_ALIGN_CENTER, v, 0);
    });
    lv_anim_set_ready_cb(&anim, [](lv_anim_t *a) {
      auto *self = static_cast<SaboScreenBoot *>(a->var);
      lv_anim_delete(self->wheel_anim_, self->wheel_anim_->exec_cb);
    });
    lv_anim_set_deleted_cb(&anim, [](lv_anim_t *a) {
      auto *self = static_cast<SaboScreenBoot *>(a->var);
      self->mower_anim_ = nullptr;  // Clear reference when deleted
      self->boot_stage_ = BootStage::ANIM_COMPLETED;
    });

    boot_stage_ = BootStage::ANIM_STARTED;
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
        &anim, [](void *var, int32_t v) { lv_obj_set_style_transform_angle(static_cast<lv_obj_t *>(var), v, 0); });
    lv_anim_set_completed_cb(&anim, [](lv_anim_t *a) {
      auto *self = static_cast<SaboScreenBoot *>(a->var);
      self->wheel_anim_ = nullptr;  // Clear reference when done
    });
    lv_anim_set_deleted_cb(&anim, [](lv_anim_t *a) {
      auto *self = static_cast<SaboScreenBoot *>(a->var);
      self->wheel_anim_ = nullptr;  // Clear reference when deleted
    });
    wheel_anim_ = lv_anim_start(&anim);
  }

  BootStage GetBootStage() const {
    return boot_stage_;
  }

  // Funktion zum Aktualisieren des Boot-Fortschritts
  void SetBootProgress(int32_t progress) {
    if (status_bar_) {
      status_bar_->SetValue(progress);
    }
  }

  // Boot-Stadium mit automatischem Fortschritt aktualisieren
  void SetBootStage(BootStage stage) {
    boot_stage_ = stage;

    // Automatisch Fortschritt basierend auf Stadium setzen
    int32_t progress = 0;
    const char *stage_text = nullptr;

    switch (stage) {
      case BootStage::INIT:
        progress = 0;
        stage_text = "Initializing...";
        break;
      case BootStage::CHECK_HARDWARE:
        progress = 20;
        stage_text = "Hardware Check";
        break;
      case BootStage::INIT_SENSORS:
        progress = 40;
        stage_text = "Sensors Online";
        break;
      case BootStage::INIT_MOTORS:
        progress = 60;
        stage_text = "Motors Ready";
        break;
      case BootStage::INIT_NAVIGATION:
        progress = 80;
        stage_text = "Navigation OK";
        break;
      case BootStage::INIT_COMPLETE:
        progress = 100;
        stage_text = "Ready to Mow!";
        break;
      case BootStage::ANIM_STARTED:
        progress = 100;
        stage_text = "Starting...";
        break;
      case BootStage::ANIM_COMPLETED:
        progress = 100;
        stage_text = "Complete";
        break;
    }

    // Update progress bar with custom text
    if (status_bar_) {
      status_bar_->SetValue(progress, stage_text, true);  // Mit Animation
    }
  }

  // Test-Funktion um die Progressbar zu testen
  void TestProgressBar() {
    if (!status_bar_) {
      return;
    }

    // Test verschiedene Fortschrittswerte um intelligente Positionierung zu zeigen
    const char *test_texts[] = {
        "Loading...",     // 10%
        "Hardware OK",    // 30%
        "50%",            // 50%
        "Sensors Ready",  // 70%
        "Complete!"       // 90%
    };

    int progress_values[] = {10, 30, 50, 70, 90};

    for (int i = 0; i < 5; i++) {
      status_bar_->SetValue(progress_values[i], test_texts[i]);
    }
  }

  // Demo-Funktion um Boot-Fortschritt zu simulieren
  void SimulateBootSequence() {
    // Stop any existing timer
    if (boot_timer_) {
      lv_timer_delete(boot_timer_);
      boot_timer_ = nullptr;
    }

    // Reset step counter
    boot_step_ = 0;

    // Create new timer
    boot_timer_ = lv_timer_create(boot_timer_callback, 800, this);
    if (boot_timer_) {
      lv_timer_set_user_data(boot_timer_, this);
    }
  }

  // Static callback function for boot timer
  static void boot_timer_callback(lv_timer_t *timer) {
    void *user_data = lv_timer_get_user_data(timer);
    SaboScreenBoot *self = static_cast<SaboScreenBoot *>(user_data);
    if (!self) return;

    BootStage stages[] = {BootStage::INIT,        BootStage::CHECK_HARDWARE,  BootStage::INIT_SENSORS,
                          BootStage::INIT_MOTORS, BootStage::INIT_NAVIGATION, BootStage::INIT_COMPLETE};

    if (self->boot_step_ < static_cast<int>(sizeof(stages) / sizeof(stages[0]))) {
      self->SetBootStage(stages[self->boot_step_]);
      self->boot_step_++;
    } else {
      // Boot abgeschlossen, Animation starten
      self->StartForwardAnimation();
      lv_timer_delete(timer);
      self->boot_timer_ = nullptr;
      self->boot_step_ = 0;
    }
  }

 private:
  BootStage boot_stage_ = BootStage::INIT;

  lv_obj_t *mower_ = nullptr;
  lv_obj_t *wheel_ = nullptr;

  lv_anim_t *mower_anim_ = nullptr;
  lv_anim_t *wheel_anim_ = nullptr;

  // Status-Bar Widget für Boot-Fortschritt
  WidgetTextBar *status_bar_ = nullptr;

  // Timer für Boot-Simulation
  lv_timer_t *boot_timer_ = nullptr;
  int boot_step_ = 0;
};

}  // namespace xbot::driver::ui::lvgl::sabo

#endif  // LVGL_SABO_SCREEN_BOOT_HPP_
