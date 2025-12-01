/**
 * @file widget_textbar.hpp
 * @author Apehaenger (joerg@ebeling.ws)
 * @brief LVGL 9.3 text-progress-bar widget for OpenMower project
 * @version 1.0
 * @date 2025-07-23
 */
#ifndef LVGL_WIDGET_TEXTBAR_HPP_
#define LVGL_WIDGET_TEXTBAR_HPP_

#include <lvgl.h>

namespace xbot::driver::ui::lvgl {

class WidgetTextBar {
 public:
  /**
   * @brief Constructor with configuration options
   * @param parent Parent object to attach to (use nullptr for current screen)
   * @param format_string sprintf-style format string (e.g., "🔋 %d%%", "GPS: %d")
   * @param align Widget alignment on parent
   * @param x_ofs X offset from alignment position
   * @param y_ofs Y offset from alignment position
   * @param w Widget width
   * @param h Widget height
   * @param font Font to use for text (default: LV_FONT_DEFAULT)
   */
  WidgetTextBar(lv_obj_t* parent, const char* format_string, lv_align_t align = LV_ALIGN_CENTER, lv_coord_t x_ofs = 0,
                lv_coord_t y_ofs = 0, lv_coord_t w = 200, lv_coord_t h = 30, const lv_font_t* font = LV_FONT_DEFAULT)
      : format_string_(format_string), font_(font) {
    // Create bar widget with specified parent or screen
    lv_obj_t* parent_obj = parent ? parent : lv_screen_active();
    bar_ = lv_bar_create(parent_obj);

    // Register event callback for text overlay
    lv_obj_add_event_cb(bar_, WidgetTextBar::event_bar_label_cb, LV_EVENT_DRAW_MAIN_END, this);

    // Position the widget
    lv_obj_set_size(bar_, w, h);
    lv_obj_align(bar_, align, x_ofs, y_ofs);

    // Set reasonable default range
    lv_bar_set_range(bar_, 0, 100);
  }

  ~WidgetTextBar() = default;

  /**
   * @brief Set the font for the text bar
   */
  void SetFont(const lv_font_t* font) {
    font_ = font;
    if (bar_) lv_obj_invalidate(bar_);
  }

  /**
   * @brief Set progress bar value with optional animation and custom text
   * @param value New value within the set range
   * @param custom_text Optional custom text (nullptr to use format string)
   */
  void SetValue(int32_t value, const char* custom_text = nullptr) {
    if (!bar_) return;

    lv_bar_set_value(bar_, value, LV_ANIM_ON);

    if (custom_text) {
      SetText(custom_text);
    }
  }

  /**
   * @brief Set a custom text (overrides format string)
   * @param text Custom text to display (e.g., "Loading...", "85%", "12.5V")
   */
  void SetText(const char* text) {
    if (!bar_ || !text) return;
    lv_strlcpy(text_buffer_, text, sizeof(text_buffer_));
    lv_obj_invalidate(bar_);  // Trigger redraw
  }

  /**
   * @brief Set progress bar value with formatted text (simple version)
   * @param value New value within the set range
   * @param format Printf-style format string with one integer placeholder
   * @param format_value Value to insert into format string
   */
  void SetValueFormatted(int32_t value, const char* format, int32_t format_value) {
    if (!bar_ || !format) return;

    // Format the text with simple sprintf
    static char formatted_buffer[64];
    lv_snprintf(formatted_buffer, sizeof(formatted_buffer), format, format_value);

    SetValue(value, formatted_buffer);
  }

  /**
   * @brief Set value range for the progress bar
   * @param min Minimum value
   * @param max Maximum value
   */
  void SetRange(int32_t min, int32_t max) {
    if (!bar_) return;
    lv_bar_set_range(bar_, min, max);
  }

  void Hide() {
    if (bar_) lv_obj_add_flag(bar_, LV_OBJ_FLAG_HIDDEN);
  }

  void Show() {
    if (bar_) lv_obj_clear_flag(bar_, LV_OBJ_FLAG_HIDDEN);
  }

 private:
  lv_obj_t* bar_ = nullptr;
  lv_style_t bar_style_bg_, bar_style_indic_;
  const char* format_string_;
  const lv_font_t* font_ = LV_FONT_DEFAULT;
  char text_buffer_[64] = {0};

  /**
   * @brief Event callback for drawing text overlay on progress bar
   * Uses LVGL 9.3 APIs with intelligent text positioning and color inversion for monochrome displays
   */
  static void event_bar_label_cb(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target_obj(e);
    if (!obj) return;

    // Get WidgetTextBar instance from user data
    WidgetTextBar* self = static_cast<WidgetTextBar*>(lv_event_get_user_data(e));
    if (!self) return;

    // Get the custom text or format string from instance
    const char* text_data = (self->text_buffer_[0] != '\0') ? self->text_buffer_ : self->format_string_;
    if (!text_data) return;

    // Initialize label descriptor with LVGL 9.3 API
    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.font = self->font_ ? self->font_ : LV_FONT_DEFAULT;

    // Buffer for the final text
    constexpr size_t BUFFER_SIZE = 64;
    char buf[BUFFER_SIZE];

    // Check if text_data contains a format specifier (%)
    if (strchr(text_data, '%') != nullptr) {
      // It's a format string - use bar value
      int32_t bar_value = lv_bar_get_value(obj);
      lv_snprintf(buf, BUFFER_SIZE, text_data, bar_value);
    } else {
      // It's plain text - use as is
      lv_strlcpy(buf, text_data, BUFFER_SIZE);
    }

    // Calculate text dimensions
    lv_point_t txt_size;
    lv_text_get_size(&txt_size, buf, label_dsc.font, label_dsc.letter_space, label_dsc.line_space, LV_COORD_MAX,
                     label_dsc.flag);

    // Get bar coordinates and calculate indicator area
    lv_area_t bar_coords, txt_area;
    lv_obj_get_coords(obj, &bar_coords);

    // Calculate actual indicator width for intelligent text positioning
    int32_t bar_range = lv_bar_get_max_value(obj) - lv_bar_get_min_value(obj);
    int32_t current_value = lv_bar_get_value(obj) - lv_bar_get_min_value(obj);
    int32_t bar_width = lv_area_get_width(&bar_coords);
    int32_t indic_width = bar_range > 0 ? (bar_width * current_value) / bar_range : 0;

    // Prefer text right of the inidicator if fits, otherwise left
    constexpr int32_t MIN_TEXT_MARGIN = 8;
    int32_t text_x = 0;
    int32_t text_y = bar_coords.y1 + (lv_area_get_height(&bar_coords) - txt_size.y) / 2;

    // Check if text fits outside the indicator area
    if ((bar_width - indic_width) >= txt_size.x + MIN_TEXT_MARGIN) {
      text_x = bar_coords.x1 + indic_width + ((bar_width - indic_width) - txt_size.x) / 2;
      label_dsc.color = lv_color_black();
    } else if (indic_width >= txt_size.x + MIN_TEXT_MARGIN) {
      // Text fits in the indicator (left)
      text_x = bar_coords.x1 + (indic_width - txt_size.x) / 2;
      label_dsc.color = lv_color_white();
    } else {
      // Fallback: Text centered over the whole bar, color depending on fill level
      text_x = bar_coords.x1 + (bar_width - txt_size.x) / 2;
      label_dsc.color = (indic_width > bar_width / 2) ? lv_color_white() : lv_color_black();
    }

    txt_area.x1 = text_x;
    txt_area.x2 = text_x + txt_size.x - 1;
    txt_area.y1 = text_y;
    txt_area.y2 = text_y + txt_size.y - 1;

    label_dsc.text = buf;
    label_dsc.text_local = true;

    // Get layer for drawing
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_draw_label(layer, &label_dsc, &txt_area);
  }

  /**
   * @brief Get current progress bar value
   * @return Current value
   */
  int32_t GetValue() const {
    return bar_ ? lv_bar_get_value(bar_) : 0;
  }
};

}  // namespace xbot::driver::ui::lvgl

#endif
