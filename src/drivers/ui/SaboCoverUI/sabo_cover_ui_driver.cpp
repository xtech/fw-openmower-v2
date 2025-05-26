//
// Created by Apehaenger on 4/19/25.
//
#include "sabo_cover_ui_driver.hpp"

#include <ulog.h>

#include "../robots/src/Sabo/robot_ex.hpp"
#include "globals.hpp"

bool SaboCoverUIDriver::Init() {
  // Init SPI pins
  palSetLineMode(LINE_UI_SCK, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_FLOATING);
  palSetLineMode(LINE_UI_MISO, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_FLOATING);
  palSetLineMode(LINE_UI_MOSI, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_FLOATING);

  // Init GPIOs for chip control
  palSetLineMode(LINE_UI_LATCH_LOAD, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_FLOATING);
  palSetLineMode(LINE_UI_LED_OE, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_FLOATING);
  palSetLineMode(LINE_UI_BTN_CE,
                 PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 |
                     PAL_STM32_PUPDR_PULLDOWN);  // FIXME: CoverUI doesn't has any pull-up/down resistors?

  // Configure SPI
  spi_ = &SPI_UI;
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
  if (spiStart(spi_, &spi_cfg_) != MSG_OK) {
    ULOG_ERROR("Error starting UI-SPI");
    return false;
  }

  return true;
}

void SaboCoverUIDriver::LatchLoad() {
  uint8_t button_data = 0;
  uint8_t tx_data = current_leds_ | ((current_button_row_ + 1) << 5);  // Bit5/6 for row0/1

  // Load 74HC165 shift register (the very first load will load nothing, because row is not set yet. Who cares?)
  palWriteLine(LINE_UI_LATCH_LOAD, PAL_LOW);  // /PL (parallel load) Pulse
  chThdSleepMicroseconds(1);
  palWriteLine(LINE_UI_LATCH_LOAD, PAL_HIGH);

  // SPI transfer LEDs and read button for previously set row
  spiAcquireBus(spi_);
  palWriteLine(LINE_UI_LATCH_LOAD, PAL_HIGH);    // Latch HEF4794BT (STR = HIGH)
  palWriteLine(LINE_UI_BTN_CE, PAL_LOW);         // HC165 /CE
  spiExchange(spi_, 1, &tx_data, &button_data);  // Send data and receive button data
  palWriteLine(LINE_UI_LATCH_LOAD, PAL_LOW);     // Stop latching (STR = LOW)
  palWriteLine(LINE_UI_BTN_CE, PAL_HIGH);
  spiReleaseBus(spi_);

  // Buffer / Shift & buffer depending on current row
  if (current_button_row_ == 0) {
    button_states_ = (button_states_ & 0xFF00) | button_data;
  } else {
    button_states_ = (button_states_ & 0x00FF) | (button_data << 8);
  }

  // Alternate between row0 and row1
  current_button_row_ ^= 1;
}

void SaboCoverUIDriver::EnableOutput() {
  // TODO: OE could also be used with a PWM signal to dim the LEDs

  if (carrier_board_info.version_major <= 0 && carrier_board_info.version_minor <= 1) {
    // Sabo v0.1 has an HEF4794BT OE driver which inverts the signal. Newer boards will not have this driver anymore.
    palWriteLine(LINE_UI_LED_OE, PAL_LOW);
  } else {
    palWriteLine(LINE_UI_BTN_CE, PAL_HIGH);
  }
}

void SaboCoverUIDriver::SetLEDs(uint8_t leds) {
  current_leds_ = leds & 0x1F;  // LEDs are on the lower 5 bits
}

uint16_t SaboCoverUIDriver::GetRawButtonStates() const {
  return button_states_;
}
