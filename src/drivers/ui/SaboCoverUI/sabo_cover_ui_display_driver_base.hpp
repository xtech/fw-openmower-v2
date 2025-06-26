//
// Created by Apehaenger on 6/23/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_DISPLAY_DRIVER_BASE_HPP
#define OPENMOWER_SABO_COVER_UI_DISPLAY_DRIVER_BASE_HPP

#include "sabo_cover_ui_types.hpp"

namespace xbot::driver::ui {

/* Controller prospects:
 *
 * ST7565 65x132
 * ST7567
 * ST75256
 * ST75256
 * UC1701
 * UC1601
 * S6B0724
 * RA6963
 * SSD1309
 */

class SaboCoverUIDisplayDriverBase {
 public:
  SaboCoverUIDisplayDriverBase(SPIDriver* spi_instance, const LCDPins& lcd_pins, const uint16_t lcd_width,
                               const uint16_t lcd_height)
      : spi_instance_(spi_instance), lcd_pins_(lcd_pins), lcd_width_(lcd_width), lcd_height_(lcd_height){};

  virtual bool Init() {
    // Init the LCD pins
    palSetLineMode(lcd_pins_.cs, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_PULLUP);
    palWriteLine(lcd_pins_.cs, PAL_HIGH);

    palSetLineMode(lcd_pins_.dc, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_PULLUP);

    palSetLineMode(lcd_pins_.rst, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_PULLUP);
    palWriteLine(lcd_pins_.rst, PAL_HIGH);

    return true;
  }

  virtual void InitController() = 0;  // Initialize COG Controller
  virtual void setVBiasPotentiometer(uint8_t data) = 0;

 protected:
  SPIDriver* spi_instance_;
  const LCDPins lcd_pins_;

  const uint16_t lcd_width_;
  const uint16_t lcd_height_;

  // clang-format off
  // Chip Select (CS)
  inline void SetCS() { palWriteLine(lcd_pins_.cs, PAL_HIGH); }
  inline void ClrCS() { palWriteLine(lcd_pins_.cs, PAL_LOW); }
  // Data/Command (D/C)
  inline void SetDC() { palWriteLine(lcd_pins_.dc, PAL_HIGH); }
  inline void ClrDC() { palWriteLine(lcd_pins_.dc, PAL_LOW); }
  // Reset (RST)
  inline void SetRST() { palWriteLine(lcd_pins_.rst, PAL_HIGH); }
  inline void ClrRST() { palWriteLine(lcd_pins_.rst, PAL_LOW); }
  // Some comfort shortcuts
  inline void SetCmdMode() { ClrDC(); }
  inline void SetDataMode() { SetDC(); }
  // clang-format on

  void SendCommand(const uint8_t cmd) {
    spiAcquireBus(spi_instance_);
    ClrCS();
    SetCmdMode();
    spiSend(spi_instance_, 1, &cmd);
    SetCS();
    spiReleaseBus(spi_instance_);
  }

  void SendCommand(const uint8_t* data, size_t size) {
    spiAcquireBus(spi_instance_);
    ClrCS();
    SetCmdMode();
    spiSend(spi_instance_, size, data);
    SetCS();
    spiReleaseBus(spi_instance_);
  }

  void SendData(uint16_t data) {
    spiAcquireBus(spi_instance_);
    ClrCS();
    SetDataMode();
    uint8_t part = (uint8_t)(data >> 8);
    spiSend(spi_instance_, 1, &part);
    part = (uint8_t)data;
    spiSend(spi_instance_, 1, &part);
    SetCS();
    spiReleaseBus(spi_instance_);
  }

  void SendData(const uint8_t* data, size_t size) {
    spiAcquireBus(spi_instance_);
    ClrCS();
    SetDataMode();
    spiSend(spi_instance_, size, data);
    SetCS();
    spiReleaseBus(spi_instance_);
  }
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_DISPLAY_DRIVER_STX_HPP
