#include "../include/sabo_robot.hpp"

#include <services.hpp>

#define LINE_EMERGENCY_1 LINE_GPIO13  // Front left wheel lift (Hall)
#define LINE_EMERGENCY_2 LINE_GPIO12  // Front right wheel lift (Hall)
#define LINE_EMERGENCY_3 LINE_GPIO11  // Top stop button (Hall)
#define LINE_EMERGENCY_4 LINE_GPIO10  // Back-handle stop (Capacitive)

void SaboRobot::InitPlatform() {
  // Front left wheel lift (Hall)
  emergency_driver_.AddInput({.gpio_line = LINE_EMERGENCY_1,
                              .invert = false,
                              .active_since = 0,
                              .timeout_duration = TIME_MS2I(2500),
                              .active = false});
  // Front right wheel lift (Hall)
  emergency_driver_.AddInput({.gpio_line = LINE_EMERGENCY_2,
                              .invert = false,
                              .active_since = 0,
                              .timeout_duration = TIME_MS2I(2500),
                              .active = false});
  // Top stop button (Hall)
  emergency_driver_.AddInput({.gpio_line = LINE_EMERGENCY_3,
                              .invert = false,
                              .active_since = 0,
                              .timeout_duration = TIME_MS2I(10),
                              .active = false});
  // Back-handle stop (Capacitive)
  emergency_driver_.AddInput({.gpio_line = LINE_EMERGENCY_4,
                              .invert = false,
                              .active_since = 0,
                              .timeout_duration = TIME_MS2I(10),
                              .active = false});
  emergency_driver_.Start();

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
