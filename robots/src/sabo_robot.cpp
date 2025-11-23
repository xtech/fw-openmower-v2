#include "../include/sabo_robot.hpp"

#include <services.hpp>

using namespace xbot::driver::ui::sabo;
using namespace xbot::driver::sabo;

SaboRobot::SaboRobot()
    : hardware_config([]() -> config::HardwareConfig {
        // Hardware detection logic - must be in initializer list because hardware_config is const
        types::HardwareVersion version;

        if (carrier_board_info.version_major == 0) {
          switch (carrier_board_info.version_minor) {
            case 1: version = types::HardwareVersion::V0_1; break;
            case 2: version = types::HardwareVersion::V0_2; break;
            case 3: version = types::HardwareVersion::V0_3; break;
          }
        }

        const auto index = static_cast<uint8_t>(version);
        return config::HARDWARE_CONFIGS[index];
      }()) {
}

void SaboRobot::InitPlatform() {
  InitMotors();
  charger_.setI2C(&I2CD1);
  power_service.SetDriver(&charger_);
  input_service.RegisterInputDriver("sabo", &sabo_input_driver_);

  // CoverUI
  CoverUICfg cui_cfg;
  if (carrier_board_info.version_major == 0 && carrier_board_info.version_minor == 1) {
    // HW v0.1
    cui_cfg = {.cabo_cfg = {.spi = {.instance = &SPID1,
                                    .pins = {.sck = LINE_SPI1_SCK, .miso = LINE_SPI1_MISO, .mosi = LINE_SPI1_MOSI}},
                            .pins = {.latch_load = LINE_GPIO9, .oe = LINE_GPIO8, .btn_cs = LINE_GPIO1}},
               .lcd_cfg = {}};
  } else {
    // HW v0.2 and later
    cui_cfg = {
        .cabo_cfg = {.spi = {.instance = &SPID1,
                             .pins = {.sck = LINE_SPI1_SCK, .miso = LINE_SPI1_MISO, .mosi = LINE_SPI1_MOSI}},
                     .pins = {.latch_load = LINE_GPIO9, .inp_cs = LINE_GPIO1}},
        .lcd_cfg =
            {.spi = {.instance = &SPID1,
                     .pins = {.sck = LINE_SPI1_SCK, .miso = LINE_SPI1_MISO, .mosi = LINE_SPI1_MOSI, .cs = LINE_GPIO5}},
             .pins = {.dc = LINE_AGPIO4, .rst = LINE_UART7_TX, .backlight = LINE_GPIO3}},
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
