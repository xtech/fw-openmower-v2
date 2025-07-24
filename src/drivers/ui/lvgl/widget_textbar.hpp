/**
 * @file widget_textbar.hpp
 * @author Apehaenger (joerg@ebeling.ws)
 * @brief Modern LVGL 9.3 text-progress-bar widget for OpenMower project
 * @version 1.0
 * @date 2025-07-23
 *
 * @copyright Copyright (c) 2025
 */
#ifndef LVGL_WIDGET_TEXTBAR_HPP_
#define LVGL_WIDGET_TEXTBAR_HPP_

#include <lvgl.h>

namespace xbot::driver::ui::lvgl {

/**
 * @brief Modern event callback for drawing text overlay on progress bar
 * Uses LVGL 9.3 APIs with intelligent text positioning and color inversion
 */
static void event_bar_label_cb(lv_event_t* e) {
  lv_obj_t* obj = lv_event_get_target_obj(e);
  if (!obj) return;

  // Get the custom text or format string from user data
  const char* text_data = static_cast<const char*>(lv_obj_get_user_data(obj));
  if (!text_data) return;

  // Initialize modern label descriptor with LVGL 9.3 API
  lv_draw_label_dsc_t label_dsc;
  lv_draw_label_dsc_init(&label_dsc);
  label_dsc.font = LV_FONT_DEFAULT;

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

  // Calculate remaining space on both sides
  int32_t free_left = indic_width;
  int32_t free_right = bar_width - indic_width;

  // Determine best text position based on available space
  constexpr int32_t MIN_TEXT_MARGIN = 8;  // Minimum margin from edges

  if (free_left > free_right + txt_size.x / 2) {
    // More space on left side (in indicator) - position text left-center in indicator
    if (free_left >= txt_size.x + MIN_TEXT_MARGIN) {
      txt_area.x1 = bar_coords.x1 + (free_left - txt_size.x) / 2;
      label_dsc.color = lv_color_white();  // White text on colored indicator
    } else {
      // Not enough space in indicator, position in right area
      txt_area.x1 = bar_coords.x1 + indic_width + (free_right - txt_size.x) / 2;
      label_dsc.color = lv_color_black();  // Black text on background
    }
  } else if (free_right > free_left + txt_size.x / 2) {
    // More space on right side (background) - position text right-center in background
    if (free_right >= txt_size.x + MIN_TEXT_MARGIN) {
      txt_area.x1 = bar_coords.x1 + indic_width + (free_right - txt_size.x) / 2;
      label_dsc.color = lv_color_black();  // Black text on background
    } else {
      // Not enough space in background, position in left area
      txt_area.x1 = bar_coords.x1 + (free_left - txt_size.x) / 2;
      label_dsc.color = lv_color_white();  // White text on colored indicator
    }
  } else {
    // Similar space on both sides - choose based on indicator coverage
    if (indic_width >= bar_width / 2) {
      // Indicator covers more than half - prefer indicator area with white text
      txt_area.x1 = bar_coords.x1 + (free_left - txt_size.x) / 2;
      label_dsc.color = lv_color_white();
    } else {
      // Background area larger - prefer background with black text
      txt_area.x1 = bar_coords.x1 + indic_width + (free_right - txt_size.x) / 2;
      label_dsc.color = lv_color_black();
    }
  }

  // Ensure text doesn't go outside bar bounds
  if (txt_area.x1 < bar_coords.x1 + MIN_TEXT_MARGIN) {
    txt_area.x1 = bar_coords.x1 + MIN_TEXT_MARGIN;
  }
  if (txt_area.x1 + txt_size.x > bar_coords.x2 - MIN_TEXT_MARGIN) {
    txt_area.x1 = bar_coords.x2 - txt_size.x - MIN_TEXT_MARGIN;
  }

  // Finalize text area coordinates
  txt_area.x2 = txt_area.x1 + txt_size.x - 1;
  txt_area.y1 = bar_coords.y1 + (lv_area_get_height(&bar_coords) - txt_size.y) / 2;
  txt_area.y2 = txt_area.y1 + txt_size.y - 1;  // Set text properties and render
  label_dsc.text = buf;
  label_dsc.text_local = true;

  // Get layer for drawing
  lv_layer_t* layer = lv_event_get_layer(e);
  if (layer) {
    lv_draw_label(layer, &label_dsc, &txt_area);
  }
}
class WidgetTextBar {
 public:
  /**
   * @brief Configuration for text positioning behavior
   */
  enum class TextPosition {
    AUTO,           ///< Automatically position text inside or outside based on available space
    INSIDE_RIGHT,   ///< Force text inside indicator on the right
    INSIDE_LEFT,    ///< Force text inside indicator on the left
    INSIDE_CENTER,  ///< Force text inside indicator centered
    OUTSIDE_RIGHT,  ///< Force text outside indicator on the right
    OUTSIDE_LEFT    ///< Force text outside indicator on the left
  };

  /**
   * @brief Modern constructor with enhanced configuration options
   * @param parent Parent object to attach to (use nullptr for current screen)
   * @param format_string sprintf-style format string (e.g., "🔋 %d%%", "GPS: %d")
   * @param align Widget alignment on parent
   * @param x_ofs X offset from alignment position
   * @param y_ofs Y offset from alignment position
   * @param w Widget width
   * @param h Widget height
   * @param text_pos Text positioning behavior (default: AUTO)
   */
  WidgetTextBar(lv_obj_t* parent, const char* format_string, lv_align_t align = LV_ALIGN_CENTER, lv_coord_t x_ofs = 0,
                lv_coord_t y_ofs = 0, lv_coord_t w = 200, lv_coord_t h = 30, TextPosition text_pos = TextPosition::AUTO)
      : format_string_(format_string), text_position_(text_pos) {
    // Initialize modern bar styles for LVGL 9.3
    init_styles();

    // Create bar widget with specified parent or screen
    lv_obj_t* parent_obj = parent ? parent : lv_screen_active();
    bar_ = lv_bar_create(parent_obj);
    if (!bar_) return;  // Safety check

    // Store format string in user data for callback access
    lv_obj_set_user_data(bar_, const_cast<char*>(format_string_));

    // Apply styles with modern LVGL 9.3 approach
    setup_bar_appearance(w, h);

    // Register modern event callback for text overlay
    lv_obj_add_event_cb(bar_, event_bar_label_cb, LV_EVENT_DRAW_MAIN_END, nullptr);

    // Position the widget
    lv_obj_set_size(bar_, w, h);
    lv_obj_align(bar_, align, x_ofs, y_ofs);

    // Set reasonable default range
    lv_bar_set_range(bar_, 0, 100);
  }

  /**
   * @brief Legacy constructor for backward compatibility
   */
  WidgetTextBar(const char* format_string, lv_align_t align = LV_ALIGN_CENTER, lv_coord_t x_ofs = 0,
                lv_coord_t y_ofs = 0, lv_coord_t w = 200, lv_coord_t h = 30, TextPosition text_pos = TextPosition::AUTO)
      : WidgetTextBar(nullptr, format_string, align, x_ofs, y_ofs, w, h, text_pos) {
  }

  /**
   * @brief Destructor - LVGL handles object cleanup automatically
   */
  ~WidgetTextBar() = default;

  /**
   * @brief Set a custom text (overrides format string)
   * @param text Custom text to display (e.g., "Loading...", "85%", "12.5V")
   */
  void SetText(const char* text) {
    if (!bar_ || !text) return;

    // Store the custom text in user data
    // We need to manage the string memory ourselves
    static char text_buffer[64];
    lv_strlcpy(text_buffer, text, sizeof(text_buffer));
    lv_obj_set_user_data(bar_, text_buffer);
    lv_obj_invalidate(bar_);  // Trigger redraw
  }

  /**
   * @brief Set progress bar value with optional animation and custom text
   * @param value New value within the set range
   * @param custom_text Optional custom text (nullptr to use format string)
   * @param animate Enable/disable animation (default: no animation)
   */
  void SetValue(int32_t value, const char* custom_text = nullptr, bool animate = false) {
    if (!bar_) return;

    lv_bar_set_value(bar_, value, animate ? LV_ANIM_ON : LV_ANIM_OFF);

    if (custom_text) {
      SetText(custom_text);
    }
  }

  /**
   * @brief Set progress bar value with formatted text (simple version)
   * @param value New value within the set range
   * @param format Printf-style format string with one integer placeholder
   * @param format_value Value to insert into format string
   * @param animate Enable/disable animation (default: no animation)
   */
  void SetValueFormatted(int32_t value, const char* format, int32_t format_value, bool animate = false) {
    if (!bar_ || !format) return;

    // Format the text with simple sprintf
    static char formatted_buffer[64];
    lv_snprintf(formatted_buffer, sizeof(formatted_buffer), format, format_value);

    SetValue(value, formatted_buffer, animate);
  }

  /**
   * @brief Reset to use original format string behavior
   */
  void ResetToFormatString() {
    if (!bar_) return;
    lv_obj_set_user_data(bar_, const_cast<char*>(format_string_));
    lv_obj_invalidate(bar_);
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

  /**
   * @brief Get current progress bar value
   * @return Current value
   */
  int32_t GetValue() const {
    return bar_ ? lv_bar_get_value(bar_) : 0;
  }

  /**
   * @brief Update the format string for text display
   * @param new_format New sprintf-style format string
   */
  void SetFormat(const char* new_format) {
    if (!bar_ || !new_format) return;
    format_string_ = new_format;
    lv_obj_set_user_data(bar_, const_cast<char*>(format_string_));
    lv_obj_invalidate(bar_);  // Trigger redraw
  }

  /**
   * @brief Change text positioning behavior
   * @param pos New text positioning mode
   */
  void SetTextPosition(TextPosition pos) {
    text_position_ = pos;
    if (bar_) lv_obj_invalidate(bar_);  // Trigger redraw
  }

  /**
   * @brief Add object flag (e.g., LV_OBJ_FLAG_HIDDEN)
   */
  void AddFlag(lv_obj_flag_t flag) {
    if (bar_) lv_obj_add_flag(bar_, flag);
  }

  /**
   * @brief Clear object flag
   */
  void ClearFlag(lv_obj_flag_t flag) {
    if (bar_) lv_obj_clear_flag(bar_, flag);
  }

  /**
   * @brief Get underlying LVGL object for advanced customization
   * @return Pointer to LVGL bar object (can be null)
   */
  lv_obj_t* GetLvglObject() {
    return bar_;
  }

  /**
   * @brief Get underlying LVGL object (const version)
   */
  const lv_obj_t* GetLvglObject() const {
    return bar_;
  }

 private:
  lv_obj_t* bar_ = nullptr;
  lv_style_t bar_style_bg_, bar_style_indic_;
  const char* format_string_;
  TextPosition text_position_;

  /**
   * @brief Initialize modern styles for LVGL 9.3
   */
  void init_styles() {
    // Background style with modern appearance
    lv_style_init(&bar_style_bg_);
    lv_style_set_border_color(&bar_style_bg_, lv_color_hex(0x333333));
    lv_style_set_border_width(&bar_style_bg_, 2);
    lv_style_set_border_opa(&bar_style_bg_, LV_OPA_COVER);
    lv_style_set_pad_all(&bar_style_bg_, 4);  // Modern padding
    lv_style_set_radius(&bar_style_bg_, 6);   // Slightly rounded corners
    lv_style_set_bg_color(&bar_style_bg_, lv_color_hex(0xF0F0F0));
    lv_style_set_bg_opa(&bar_style_bg_, LV_OPA_COVER);

    // Indicator style with modern gradient-like appearance
    lv_style_init(&bar_style_indic_);
    lv_style_set_bg_opa(&bar_style_indic_, LV_OPA_COVER);
    lv_style_set_bg_color(&bar_style_indic_, lv_color_hex(0x2196F3));  // Material Design blue
    lv_style_set_radius(&bar_style_indic_, 3);

    // Add subtle shadow effect for depth
    lv_style_set_shadow_width(&bar_style_indic_, 4);
    lv_style_set_shadow_color(&bar_style_indic_, lv_color_hex(0x000000));
    lv_style_set_shadow_opa(&bar_style_indic_, LV_OPA_20);
  }

  /**
   * @brief Setup bar appearance with modern styling
   */
  void setup_bar_appearance(lv_coord_t w, lv_coord_t h) {
    (void)w;
    if (!bar_) return;

    // Remove default styles for clean start
    lv_obj_remove_style_all(bar_);

    // Apply custom styles
    lv_obj_add_style(bar_, &bar_style_bg_, LV_PART_MAIN);
    lv_obj_add_style(bar_, &bar_style_indic_, LV_PART_INDICATOR);

    // Set size-dependent properties
    if (h <= 20) {
      // Small bar - adjust for compact display
      lv_style_set_pad_all(&bar_style_bg_, 2);
      lv_style_set_radius(&bar_style_bg_, 3);
    } else if (h >= 50) {
      // Large bar - more prominent styling
      lv_style_set_pad_all(&bar_style_bg_, 6);
      lv_style_set_radius(&bar_style_bg_, 8);
    }
  }
};

}  // namespace xbot::driver::ui::lvgl

#endif
