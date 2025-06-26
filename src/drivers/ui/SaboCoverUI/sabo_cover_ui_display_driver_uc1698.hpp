//
// Created by Apehaenger on 6/24/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_DISPLAY_DRIVER_UC1698_HPP
#define OPENMOWER_SABO_COVER_UI_DISPLAY_DRIVER_UC1698_HPP

#include "sabo_cover_ui_display_driver_base.hpp"

namespace xbot::driver::ui {

class SaboCoverUIDisplayDriverUC1698 : public SaboCoverUIDisplayDriverBase {
 public:
  SaboCoverUIDisplayDriverUC1698(SPIDriver* spi_instance, const LCDPins& lcd_pins, const uint16_t lcd_width,
                                 const uint16_t lcd_height)
      : SaboCoverUIDisplayDriverBase(spi_instance, lcd_pins, lcd_width, lcd_height){};

  bool Init() override {
    if (!SaboCoverUIDisplayDriverBase::Init()) return false;

    // Reset the display
    ClrRST();
    chThdSleepMicroseconds(3);  // t_RW >= 3uS
    SetRST();
    chThdSleepMilliseconds(10);  // t_RD >= 10ms

    return true;
  }

  void InitController() override {
    // Directly after reset set Booster, Regulator, and Power Control in that order
    SendCommand(0b00101000 | 0b0);  // [6] Set Power Control, Booster: LCD<=13nF
    // SendCommand(0b00101000 | 0b1);  // [6] Set Power Control, Booster: 13nF<LCD<=22nF
    chThdSleepMilliseconds(10);
    SendCommand(0b00101000 | 0b10);   // [6] Set Power Control, VReg: Internal Vlcd(*10)
    chThdSleepMilliseconds(100);      // Give charge pump some time to stabilize
    SendCommand(0b00100100 | 0b01);   // [5] Set Temperature Compensation (-0.05%/C)
    SendCommand(0b11000000 | 0b100);  // [18] Set LCD Mapping Control, Mirror Y
    // SendCommand(0b10100110 | 1);  // [16] Set Inverse Display. Will save inversion logic in LVGL's flush_cb()
    SendCommand(0b11010000 | 1);  // [20] Set Color Pattern to R-G-B
    // SendCommand(0b11010100 | 0b10);  // [21] Set Color Mode to RRRRR-GGGGGG-BBBBB, 64k-color
    SendCommand(0b11010100 | 0b01);   // [21] Set Color Mode to RRRR-GGGG-BBBB, 4k-color
    SendCommand(0b10101000 | 0b111);  // [17] Set Display Enable: Green Enh. Mode off, Gray Shade On, Activate
  }

  // [10] Set VBias Potentiometer
  void setVBiasPotentiometer(uint8_t data) {
    uint8_t cmd[] = {0b10000001, data};
    SendCommand(cmd, sizeof(cmd));
  }

  void FillScreen(bool black) {
    spiAcquireBus(spi_instance_);
    ClrCS();

    SetCmdMode();
    SetWindowProgramAreaRaw(0, lcd_width_ - 1, 0, lcd_height_ - 1);

    SetDataMode();
    for (uint8_t i = 0; i < lcd_height_; i++) {
      for (uint8_t j = 0; j < lcd_width_ / 3; j++) {
        SendPixelSextetRaw(black, black, black, black, black, black);
      }
    }

    SetCS();
    spiReleaseBus(spi_instance_);
  }

 protected:
  /**
   * @brief Set Window Programm Area for next display RAM write data to x1-x2, y1-y2
   * Attention: By design of my monochrome LCD, which has 3 monochrome pixel (triplet on RGB) per pixel,
   * x1 & x2+1 have to be a multiple of 3!!
   * See LVGL's rounder_cb() callback to ensure this.
   *
   * @param t_x1
   * @param t_x2
   * @param t_y1
   * @param t_y2
   * @param t_outside_mode
   */
  void SetWindowProgramAreaRaw(uint8_t t_x1, uint8_t t_x2, uint8_t t_y1, uint8_t t_y2, bool t_outside_mode = false) {
    const uint8_t data[] = {
        // Set SRAM col/row start address
        (uint8_t)(0x10 | (((t_x1 / 3) >> 4) & 0b00000111)),  // Set Column MSB Address
        (uint8_t)(0x00 | ((t_x1 / 3) & 0b00001111)),         // Set Column LSB Address
        (uint8_t)(0x70 | (t_y1 >> 4)),                       // Set Row MSB Address
        (uint8_t)(0x60 | ((uint8_t)t_y1 & 0b00001111)),      // Set Row LSB Address

        // Set Windows Program
        (0xf5), (t_y1),                                   // Starting Row Address
        (0xf7), (t_y2),                                   // Ending Row Address
        (0xf4), (uint8_t)(t_x1 / 3),                      // Starting Column Address
        (0xf6), (uint8_t)((int8_t)((t_x2 + 1) / 3) - 1),  // Ending Column Address
        (uint8_t)(0xf8 | t_outside_mode)                  // Windows Program Mode (0 = inside, 1 = outside)
    };
    spiSend(spi_instance_, sizeof(data) / sizeof(data[0]), data);
  }

  // Send Pixel Sextet sends 2*3 pixels with one command.
  // This is required for RRRR-GGGG-BBBB, 4K-color mode which need 25% less data than RRRRR-GGGGG-BBBBB, 64K-color mode.
  void SendPixelSextetRaw(bool p1, bool p2, bool p3,  // Triplet 1
                          bool p4, bool p5, bool p6   // Triplet 2
  ) {
    uint8_t buf[3];
    buf[0] = ((p1 ? 0xF : 0) << 4) | ((p2 ? 0xF : 0));
    buf[1] = ((p3 ? 0xF : 0) << 4) | ((p4 ? 0xF : 0));
    buf[2] = ((p5 ? 0xF : 0) << 4) | ((p6 ? 0xF : 0));
    spiSend(spi_instance_, 3, buf);
  }
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_DISPLAY_DRIVER_UC1698_HPP
