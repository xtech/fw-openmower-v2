#include "../include/sabo_robot.hpp"

#include <services.hpp>

void SaboRobot::InitPlatform() {
  InitMotors();
  charger_.setI2C(&I2CD1);
  power_service.SetDriver(&charger_);

  // CoverUI
  sabo::CoverUICfg cui_cfg;
  if (carrier_board_info.version_major == 0 && carrier_board_info.version_minor == 1) {
    // HW v0.1
    cui_cfg = {
        .spi_cfg = {.instance = &SPID1, .pins = {.sck = LINE_SPI1_SCK, .miso = LINE_SPI1_MISO, .mosi = LINE_SPI1_MOSI}},
        .sr_pins = {.latch_load = LINE_GPIO9, .oe = LINE_GPIO8, .btn_cs = LINE_GPIO1},
        .lcd_pins = {}};
  } else {
    // HW v0.2 and later
    cui_cfg = {
        .spi_cfg = {.instance = &SPID1, .pins = {.sck = LINE_SPI1_SCK, .miso = LINE_SPI1_MISO, .mosi = LINE_SPI1_MOSI}},
        .sr_pins = {.latch_load = LINE_GPIO9, .oe = PAL_NOLINE, .btn_cs = PAL_NOLINE, .inp_cs = LINE_GPIO1},
        .lcd_pins = {.cs = LINE_GPIO5, .dc = LINE_AGPIO4, .rst = LINE_UART7_TX},
    };
  }
  cover_ui_.Configure(cui_cfg);
  cover_ui_.Start();
}

bool SaboRobot::IsHardwareSupported() {
  // Accept Sabo 0.1|2.x boards
  if (strncmp("hw-openmower-sabo", carrier_board_info.board_id, sizeof(carrier_board_info.board_id)) == 0 &&
      strncmp("xcore", board_info.board_id, sizeof(board_info.board_id)) == 0 && board_info.version_major == 1 &&
      carrier_board_info.version_major == 0 &&
      (carrier_board_info.version_minor == 1 || carrier_board_info.version_minor == 2)) {
    return true;
  }

  return false;
}
