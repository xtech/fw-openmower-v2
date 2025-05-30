//
// Created by Apehaenger on 5/27/25.
//
#include "sabo_cover_ui_driver_base.hpp"

#include <ulog.h>

namespace xbot::driver::ui {

bool SaboCoverUIDriverBase::Init() {
  // Init control pins
  palSetLineMode(config_.control_pins.latch_load,
                 PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_FLOATING);
  palSetLineMode(config_.control_pins.oe, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_FLOATING);
  if (config_.control_pins.btn_cs != PAL_NOLINE) {
    palSetLineMode(config_.control_pins.btn_cs,
                   PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_FLOATING);
  }
  if (config_.control_pins.inp_cs != PAL_NOLINE) {
    palSetLineMode(config_.control_pins.inp_cs,
                   PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_FLOATING);
  }

  // Init SPI pins
  palSetLineMode(config_.spi_pins.sck, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_FLOATING);
  palSetLineMode(config_.spi_pins.miso, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_FLOATING);
  palSetLineMode(config_.spi_pins.mosi, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_FLOATING);

  // Configure SPI
  spi_cfg_ = {
      .circular = false,
      .slave = false,
      .data_cb = nullptr,
      .error_cb = nullptr,
      .ssline = 0,                                                     // Master mode
      .cfg1 = SPI_CFG1_MBR_0 | SPI_CFG1_MBR_1 |                        // Baudrate = FPCLK/16 (~10 MHz @ 160 MHz SPI-C)
              SPI_CFG1_DSIZE_2 | SPI_CFG1_DSIZE_1 | SPI_CFG1_DSIZE_0,  // 8-Bit (DS = 0b111)
      .cfg2 = SPI_CFG2_MASTER                                          // Master mode
  };

  // Start SPI
  if (spiStart(config_.spi_instance, &spi_cfg_) != MSG_OK) {
    ULOG_ERROR("CoverUI SPI init failed");
    return false;
  }

  return true;
}
}  // namespace xbot::driver::ui
