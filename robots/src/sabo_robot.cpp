#include "../include/sabo_robot.hpp"

#include <services.hpp>

void SaboRobot::InitPlatform() {
  InitMotors();
  charger_.setI2C(&I2CD1);
  power_service.SetDriver(&charger_);

  // CoverUI
  sabo::DriverConfig ui_config;
  if (carrier_board_info.version_major == 0 && carrier_board_info.version_minor == 1) {
    // HW v0.1
    ui_config = {
        .spi_instance = &SPID1,
        .spi_pins = {.sck = LINE_SPI1_SCK, .miso = LINE_SPI1_MISO, .mosi = LINE_SPI1_MOSI},
        .control_pins = {.latch_load = LINE_GPIO9, .oe = LINE_GPIO8, .btn_cs = LINE_GPIO1, .inp_cs = PAL_NOLINE}};
  } else {
    // HW v0.2 and later
    ui_config = {
        .spi_instance = &SPID1,
        .spi_pins = {.sck = LINE_SPI1_SCK, .miso = LINE_SPI1_MISO, .mosi = LINE_SPI1_MOSI},
        .control_pins = {.latch_load = LINE_GPIO9, .oe = LINE_GPIO8, .btn_cs = LINE_UART7_RX, .inp_cs = LINE_GPIO1}};
  }
  cover_ui_.Configure(ui_config);
  cover_ui_.Start();
}

bool SaboRobot::IsHardwareSupported() {
  // FIXME: Fix EEPROM reading and check EEPROM

  // Accept Sabo 0.1|2.x boards
  if (strncmp("hw-openmower-sabo", carrier_board_info.board_id, sizeof(carrier_board_info.board_id)) == 0 &&
      strncmp("xcore", board_info.board_id, sizeof(board_info.board_id)) == 0 && board_info.version_major == 1 &&
      carrier_board_info.version_major == 0 &&
      (carrier_board_info.version_minor == 1 || carrier_board_info.version_minor == 2)) {
    return true;
  }

  return false;
}
