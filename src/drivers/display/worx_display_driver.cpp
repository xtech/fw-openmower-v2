//
// Created by clemens on 23.06.25.
//

#include "worx_display_driver.hpp"

#include <etl/string.h>
#include <etl/to_string.h>

#include <services.hpp>

#include "ch.h"
#include "hal.h"
#include "hal_spi.h"

#define LINE_DISPLAY_A0 LINE_GPIO4
#define LINE_DISPLAY_RESET LINE_GPIO2
#define LINE_DISPLAY_CS LINE_GPIO7
#define LINE_DISPLAY_BACKLIGHT LINE_GPIO3

extern "C" uint8_t u8x8_byte_hw_spi(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
  static SPIConfig spi_config = {
      false,
      false,
      nullptr,
      nullptr,
      LINE_DISPLAY_CS,
      SPI_CFG1_MBR_DIV256 | SPI_CFG1_DSIZE_0 | SPI_CFG1_DSIZE_1 | SPI_CFG1_DSIZE_2,
      SPI_CFG2_COMM_TRANSMITTER | SPI_CFG2_CPHA | SPI_CFG2_CPOL,
  };
  const auto spi_driver = static_cast<SPIDriver *>(u8x8->user_ptr);
  uint8_t *data;
  switch (msg) {
    case U8X8_MSG_BYTE_SEND:
      data = (uint8_t *)arg_ptr;

      spiSend(spi_driver, arg_int, data);
      break;
    case U8X8_MSG_BYTE_INIT:
      spiStart(spi_driver, &spi_config);
      spiUnselect(spi_driver);
      break;
    case U8X8_MSG_BYTE_SET_DC: palWriteLine(LINE_DISPLAY_A0, arg_int > 0 ? PAL_HIGH : PAL_LOW); break;
    case U8X8_MSG_BYTE_START_TRANSFER: spiSelect(spi_driver); break;
    case U8X8_MSG_BYTE_END_TRANSFER: spiUnselect(spi_driver); break;
    default: return 0;
  }
  return 1;
}

uint8_t u8x8_gpio_and_delay_template(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
  (void)arg_ptr;
  switch (msg) {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:  // called once during init phase of u8g2/u8x8
      palSetLineMode(LINE_DISPLAY_A0, PAL_MODE_OUTPUT_PUSHPULL);
      palSetLineMode(LINE_DISPLAY_CS, PAL_MODE_OUTPUT_PUSHPULL);
      palSetLineMode(LINE_DISPLAY_RESET, PAL_MODE_OUTPUT_PUSHPULL);
      palSetLineMode(LINE_DISPLAY_BACKLIGHT, PAL_MODE_OUTPUT_PUSHPULL);
      palWriteLine(LINE_DISPLAY_BACKLIGHT, PAL_HIGH);
      break;                   // can be used to setup pins
    case U8X8_MSG_DELAY_NANO:  // delay arg_int * 1 nano second
      chThdSleepMicroseconds(1);
      break;
    case U8X8_MSG_DELAY_100NANO:  // delay arg_int * 100 nano seconds
      chThdSleepMicroseconds(1);
      break;
    case U8X8_MSG_DELAY_10MICRO:  // delay arg_int * 10 micro seconds
      chThdSleepMicroseconds(10 * arg_int);
      break;
    case U8X8_MSG_DELAY_MILLI:  // delay arg_int * 1 milli second
      chThdSleepMilliseconds(arg_int);
      break;
    case U8X8_MSG_GPIO_DC:  // DC (data/cmd, A0, register select) pin: Output level in arg_int
      palWriteLine(LINE_DISPLAY_A0, arg_int > 0 ? PAL_HIGH : PAL_LOW);
      break;
    case U8X8_MSG_GPIO_RESET:  // Reset pin: Output level in arg_int
      palWriteLine(LINE_DISPLAY_RESET, arg_int > 0 ? PAL_HIGH : PAL_LOW);
      break;
    default:
      u8x8_SetGPIOResult(u8x8, 1);  // default return value
      break;
  }
  return 1;
}

void WorxDisplayDriver::Start() {
  processing_thread_ = chThdCreateStatic(&thd_wa_, sizeof(thd_wa_), NORMALPRIO, threadHelper, this);
}
void WorxDisplayDriver::threadFunc() {
  u8g2_Setup_st7565_nhd_c12864_1(&u8g2, U8G2_R0, u8x8_byte_hw_spi, u8x8_gpio_and_delay_template);
  u8g2.u8x8.user_ptr = &SPID2;
  u8g2_InitDisplay(&u8g2);
  u8g2_SetPowerSave(&u8g2, 0);  // wake up display
  u8g2_SetContrast(&u8g2, 55);

  u8g2_SetFont(&u8g2, u8g2_font_ncenB12_tr);
  etl::format_spec double_spec{};
  double_spec.precision(2);

  while (true) {
    chThdSleepMilliseconds(100);
    etl::string<100> text1 = "VBatt: ";
    etl::string<100> text2 = "Current: ";
    float volts = power_service.GetBatteryVolts();
    etl::to_string(volts, text1, double_spec, true);
    text1 += " V";
    float current = power_service.GetChargeCurrent();
    etl::to_string(current, text2, double_spec, true);
    text2 += " A";

    u8g2_FirstPage(&u8g2);
    do {
      u8g2_DrawStr(&u8g2, 0, 20, text1.c_str());
      u8g2_DrawStr(&u8g2, 0, 50, text2.c_str());

    } while (u8g2_NextPage(&u8g2));
  }
}
void WorxDisplayDriver::threadHelper(void *instance) {
  auto *gps_interface = static_cast<WorxDisplayDriver *>(instance);
  gps_interface->threadFunc();
}
