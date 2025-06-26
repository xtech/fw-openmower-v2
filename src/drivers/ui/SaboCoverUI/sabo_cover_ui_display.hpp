//
// Created by Apehaenger on 6/23/25.
//

/**
 * @brief Model AGG240160B05?
 *
 * 240*160 pixels, 1-bit monochrome, UC1698u controller
 *
 * PINS:
 * ID0 = 0 => ?
 * ID1 = 0 => 8-bit input data D[0,2,4,6,8,10,12,14]
 * BM[1:0] = 00 => SPI w/ 8-bit token
 * WR[1:0] = NC
 *
 */
#ifndef OPENMOWER_SABO_COVER_UI_DISPLAY_HPP
#define OPENMOWER_SABO_COVER_UI_DISPLAY_HPP

#include <ulog.h>

#include "sabo_cover_ui_display_driver_uc1698.hpp"

#define PIN_BACKLIGHT LINE_GPIO3

namespace xbot::driver::ui {

class SaboCoverUIDisplay {
 public:
  explicit SaboCoverUIDisplay(SPIDriver* spi_instance, const LCDPins& lcd_pins, const uint16_t lcd_width,
                              const uint16_t lcd_height)
      : lcd_driver_(spi_instance, lcd_pins, lcd_width, lcd_height) {
    // Constructor initializes the LCD driver with the provided SPI instance and pins
  }

  bool Init() {
    // FIXME: Backlight needs to go into a separate Class or Method with timeouts and ...
    palSetLineMode(PIN_BACKLIGHT, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_PULLUP);
    palWriteLine(PIN_BACKLIGHT, PAL_LOW);
    chThdSleepMilliseconds(200);
    palWriteLine(PIN_BACKLIGHT, PAL_HIGH);

    lcd_driver_.Init();  // Initialize the LCD driver

    return true;
  };

  void Tick() {
    static bool controller_initialized = false;
    if (!controller_initialized) {
      lcd_driver_.InitController();  // Initialize the LCD controller
      controller_initialized = true;

      lcd_driver_.setVBiasPotentiometer(80);  // FIXME: Make configurable

      auto start = chVTGetSystemTimeX();
      lcd_driver_.FillScreen(false);
      auto end = chVTGetSystemTimeX();
      ULOG_INFO("FillScreen took %u us", TIME_I2US(end - start));

      start = chVTGetSystemTimeX();
      lcd_driver_.FillScreen(true);
      end = chVTGetSystemTimeX();
      ULOG_INFO("FillScreen took %u us", TIME_I2US(end - start));
    }
  };

 protected:
  SaboCoverUIDisplayDriverUC1698 lcd_driver_;
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_DISPLAY_HPP
