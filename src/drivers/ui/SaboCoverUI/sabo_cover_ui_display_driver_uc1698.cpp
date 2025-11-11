/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sabo_cover_ui_display_driver_uc1698.cpp
 * @brief Display driver for UC1698-based LCDs
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-06-24
 */

/**
 * @brief Model AGG240160B05?
 *
 * 240*160 pixels, UC1698u controller
 *
 * The display does support gray-scale mode, and UC1698 is already configured for optimized gray-scale operation,
 * but due to it's bad quality, in fact only 1 or max. 2 gray scales are usable:
 * 0xf = black, 0xa = 50%, (0x2 = 25%), 0x0 = off
 *
 * PINS:
 * ID0 = 0 => ?
 * ID1 = 0 => 8-bit input data D[0,2,4,6,8,10,12,14]
 * BM[1:0] = 00 => SPI w/ 8-bit token
 * WR[1:0] = NC
 */

#include "sabo_cover_ui_display_driver_uc1698.hpp"

#include <ulog.h>

#include "sabo_cover_ui_defs.hpp"

namespace xbot::driver::ui {

bool SaboCoverUIDisplayDriverUC1698::Init() {
  // Init SPI pins
  if (lcd_cfg_.spi.instance != nullptr) {
    palSetLineMode(lcd_cfg_.spi.pins.sck, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_MID2);
    palSetLineMode(lcd_cfg_.spi.pins.miso, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_MID2);
    palSetLineMode(lcd_cfg_.spi.pins.mosi, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_MID2);
    if (lcd_cfg_.spi.pins.cs != PAL_NOLINE) {
      palSetLineMode(lcd_cfg_.spi.pins.cs, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2);
      palWriteLine(lcd_cfg_.spi.pins.cs, PAL_HIGH);
    }
  }

  // Init the LCD pins
  if (lcd_cfg_.pins.dc != PAL_NOLINE) {
    palSetLineMode(lcd_cfg_.pins.dc, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2);
  }
  if (lcd_cfg_.pins.rst != PAL_NOLINE) {
    palSetLineMode(lcd_cfg_.pins.rst, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2);
    palWriteLine(lcd_cfg_.pins.rst, PAL_HIGH);
  }

  // Configure SPI
  spi_config_ = {
      .circular = false,
      .slave = false,
      .data_cb = SPIDataCB,
      .error_cb = NULL,
      .ssline = lcd_cfg_.spi.pins.cs,  // Chip Select line

      // UC1698u is rated with a max frequency of 25 MHz
      .cfg1 = SPI_CFG1_MBR_1 |                                         // Baudrate = FPCLK/8 (25 MHz @ 200 MHz PLL2_P)
              SPI_CFG1_DSIZE_2 | SPI_CFG1_DSIZE_1 | SPI_CFG1_DSIZE_0,  // 8-Bit (DS = 0b111)*/
      .cfg2 = SPI_CFG2_MASTER  // Master, Mode 0 (CPOL=0, CPHA=0) = Data on rising edge
  };

  // Reset the display
  ClrRST();
  chThdSleepMicroseconds(3);  // t_RW >= 3uS
  SetRST();
  chThdSleepMilliseconds(10);  // t_RD >= 10ms

  return true;
}

void SaboCoverUIDisplayDriverUC1698::Start() {
  // Load LCD settings from file (or use defaults)
  if (!LCDSettings::Load(lcd_settings_)) {
    ULOG_WARNING("LCD settings file not found, using defaults");
  }

  thread_ = chThdCreateStatic(&wa_, sizeof(wa_), NORMALPRIO, ThreadHelper, this);
#ifdef USE_SEGGER_SYSTEMVIEW
  processing_thread_->name = "SaboCoverUIDisplayDriverUC1698";
#endif
}

void SaboCoverUIDisplayDriverUC1698::InitController() {
  // Directly after reset set Booster, Regulator, and Power Control in that order
  SendCommand(0b00101000 | 0b0);  // [6] Set Power Control, Booster: 0=LCD<=13nF, 1=13nF<LCD<=22nF
  chThdSleepMilliseconds(10);
  SendCommand(0b00101000 | 0b10);  // [6] Set Power Control, VReg: Internal Vlcd(*10)
  chThdSleepMilliseconds(100);     // Give charge pump some time to stabilize
  SendCommand(0b00100100 | static_cast<uint8_t>(lcd_settings_.temp_compensation));  // [5] Set Temperature Compensation
  SendCommand(0b11000000 | 0b100);  // [18] Set LCD Mapping Control, Mirror Y
  SendCommand(0b11010000 | 1);      // [20] Set Color Pattern to R-G-B
  SendCommand(0b11010100 | 0b01);   // [21] Set Color Mode: 1=RRRR-GGGG-BBBB, 4k-color, 2=RRRRR-GGGGGG-BBBBB, 64k-color

  SetVBiasPotentiometer(lcd_settings_.contrast);  // Apply contrast from settings

  SetDisplayEnable(true);

  ULOG_INFO("LCD initialized with settings: contrast=%d, temp_comp=%d, sleep=%d min", lcd_settings_.contrast,
            static_cast<uint8_t>(lcd_settings_.temp_compensation), lcd_settings_.auto_sleep_minutes);
}

// [10] Set VBias Potentiometer (Contrast)
void SaboCoverUIDisplayDriverUC1698::SetVBiasPotentiometer(uint8_t data) {
  uint8_t cmd[] = {0b10000001, data};
  SendCommands(cmd, sizeof(cmd));
}

// [5] Set Temperature Compensation
void SaboCoverUIDisplayDriverUC1698::SetTemperatureCompensation(sabo::settings::TempCompensation tc) {
  SendCommand(0b00100100 | static_cast<uint8_t>(tc));
}

void SaboCoverUIDisplayDriverUC1698::SetDisplayEnable(bool on) {
  // [17] Set Display Enable: Green Enh. Mode disabled, Gray Shade On, $on
  SendCommand(0b10101000 | (on ? 0b111 : 0b110));
  display_enabled_ = on;
}

void SaboCoverUIDisplayDriverUC1698::SPIDataCB(SPIDriver* spip) {
  auto& driver = SaboCoverUIDisplayDriverUC1698::Instance();
  (void)spip;

  if (driver.transfer_state_ == TransferState::FLUSH_AREA) {
    chSysLockFromISR();
    chEvtBroadcastFlagsI(&driver.event_source_, static_cast<eventmask_t>(TransferEvent::ASYNC_TX_DONE));
    chSysUnlockFromISR();
  }
}

void SaboCoverUIDisplayDriverUC1698::LVGLRounderCB(lv_event_t* e) {
  lv_area_t* area = lv_event_get_invalidated_area(e);

  // Round x axis to a multiple of 24 to fullfil:
  // - multiple of 8 requirement of LV_COLOR_FORMAT_I1
  // - multiple of 3 requirement of 3 LCD pixel form one UC1698 pixel
  area->x1 = (area->x1 / 24) * 24;
  area->x2 = ((area->x2 + 1 + 23) / 24) * 24 - 1;

  // Due to the 25% more efficient RRRR-GGGG-BBBB, 4K-color mode of UC1698 (in comparison to 64k-color mode)
  // we need to assure that the total num of send pixels is a multiple of 6
  int width = area->x2 - area->x1 + 1;
  int height = area->y2 - area->y1 + 1;
  int total = width * height;
  int rest = total % 6;

  if (rest != 0) {
    // Increase y2 so that the product becomes a multiple of 6
    int add = (6 - rest + width - 1) / width;  // min. required rows
    area->y2 += add;
  }
}

/**
 * @brief LVGL Flush Callback does only convert the I1 px_map data to our RRRR-GGGG-BBBB, 4K-color mode,
 * buffers it in our internal buffer and triggers an ASYNC_BUF_READY event.
 */
void SaboCoverUIDisplayDriverUC1698::LVGLFlushCB(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
  (void)disp;
  auto& driver = SaboCoverUIDisplayDriverUC1698::Instance();
  uint8_t* buf = driver.pending_flush_.buffer;
  // driver.profile_.flush_start = chVTGetSystemTimeX();

  int width = area->x2 - area->x1 + 1;
  int height = area->y2 - area->y1 + 1;
  int total_pixels = width * height;
  int blocks = total_pixels / 24;

  const uint8_t* src = px_map + 8;  // Skip 2*4 byte palette

  // Fill buffer with 2 pixels per byte
  size_t buf_idx = 0;
  for (int b = 0; b < blocks; b++) {
    // Build 24 Bits from 3 Bytes
    uint32_t pix24 = (src[0] << 16) | (src[1] << 8) | src[2];
    src += 3;

    // MSB = Leftmost Pixel, LSB = Rightmost pixel
    for (int i = 23; i >= 0; i -= 2) {
      bool p1 = (pix24 >> i) & 0x01;
      bool p2 = (pix24 >> (i - 1)) & 0x01;

      chDbgAssert(buf_idx < buffer_size_, "SPI TX Buffer Overflow");

      // LVGL bit true == white == LCD dot off
      buf[buf_idx++] = (p1 ? 0x00 : 0xF0) | (p2 ? 0x00 : 0x0F);
    }
  }
  driver.transfer_state_ = TransferState::QUEUED;

  driver.pending_flush_.size = buf_idx;
  driver.pending_flush_.area = *area;

  chEvtBroadcastFlags(&driver.event_source_, static_cast<eventmask_t>(TransferEvent::ASYNC_BUF_READY));
}

void SaboCoverUIDisplayDriverUC1698::LVGLFlushWaitCB(lv_display_t* disp) {
  (void)disp;
  auto& driver = SaboCoverUIDisplayDriverUC1698::Instance();

  while (driver.transfer_state_ != TransferState::IDLE) {
    chThdSleepMicroseconds(100);
  }
}

void SaboCoverUIDisplayDriverUC1698::SendCommand(const uint8_t cmd) {
  spiAcquireBus(lcd_cfg_.spi.instance);
  spiStart(lcd_cfg_.spi.instance, &spi_config_);
  spiSelect(lcd_cfg_.spi.instance);
  SetCmdMode();
  spiSend(lcd_cfg_.spi.instance, 1, &cmd);
  spiUnselect(lcd_cfg_.spi.instance);
  spiReleaseBus(lcd_cfg_.spi.instance);
}

void SaboCoverUIDisplayDriverUC1698::SendCommands(const uint8_t* data, size_t size) {
  spiAcquireBus(lcd_cfg_.spi.instance);
  spiStart(lcd_cfg_.spi.instance, &spi_config_);
  spiSelect(lcd_cfg_.spi.instance);
  SetCmdMode();
  spiSend(lcd_cfg_.spi.instance, size, data);
  spiUnselect(lcd_cfg_.spi.instance);
  spiReleaseBus(lcd_cfg_.spi.instance);
}

void SaboCoverUIDisplayDriverUC1698::SendData(const uint8_t* data, size_t size) {
  spiAcquireBus(lcd_cfg_.spi.instance);
  // ClrCS();
  SetDataMode();
  spiSend(lcd_cfg_.spi.instance, size, data);
  // SetCS();
  spiReleaseBus(lcd_cfg_.spi.instance);
}

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
void SaboCoverUIDisplayDriverUC1698::SetWindowProgramAreaRaw(uint8_t t_x1, uint8_t t_x2, uint8_t t_y1, uint8_t t_y2,
                                                             bool t_outside_mode) {
  const uint8_t data[] = {
      // Set SRAM col/row start address
      (uint8_t)(0x10 | (((t_x1 / 3) >> 4) & 0b00000111)),  // (4) Set Column MSB Address
      (uint8_t)(0x00 | ((t_x1 / 3) & 0b00001111)),         // (4) Set Column LSB Address
      (uint8_t)(0x70 | (t_y1 >> 4)),                       // (9) Set Row MSB Address
      (uint8_t)(0x60 | ((uint8_t)t_y1 & 0b00001111)),      // (9) Set Row LSB Address

      // Set Windows Program
      (0xf5), (t_y1),                                   // (31) Starting Row Address
      (0xf7), (t_y2),                                   // (33) Ending Row Address
      (0xf4), (uint8_t)(t_x1 / 3),                      // (30) Starting Column Address
      (0xf6), (uint8_t)((int8_t)((t_x2 + 1) / 3) - 1),  // (32) Ending Column Address
      (uint8_t)(0xf8 | t_outside_mode)                  // Windows Program Mode (0 = inside, 1 = outside)
  };
  spiSend(lcd_cfg_.spi.instance, sizeof(data), data);
}

void SaboCoverUIDisplayDriverUC1698::ThreadFunc() {
  InitController();  // Initialize the LCD controller (loads settings and applies them)

  event_listener_t event_listener;
  chEvtRegister(&event_source_, &event_listener, 0);

  while (true) {
    chEvtWaitAny(ALL_EVENTS);
    eventflags_t flags = chEvtGetAndClearFlags(&event_listener);

    if (flags & static_cast<eventmask_t>(TransferEvent::ASYNC_BUF_READY)) {
      // profile_.spi_start = chVTGetSystemTimeX();

      spiAcquireBus(lcd_cfg_.spi.instance);
      transfer_state_ = TransferState::SYNC;
      spiStart(lcd_cfg_.spi.instance, &spi_config_);
      spiSelect(lcd_cfg_.spi.instance);

      SetCmdMode();
      SetWindowProgramAreaRaw(pending_flush_.area.x1, pending_flush_.area.x2, pending_flush_.area.y1,
                              pending_flush_.area.y2);

      SetDataMode();
      transfer_state_ = TransferState::FLUSH_AREA;
      spiStartSend(lcd_cfg_.spi.instance, pending_flush_.size, pending_flush_.buffer);
    }

    // SPI async transfer complete event
    if (flags & static_cast<eventmask_t>(TransferEvent::ASYNC_TX_DONE)) {
      spiUnselect(lcd_cfg_.spi.instance);
      spiReleaseBus(lcd_cfg_.spi.instance);
      transfer_state_ = TransferState::IDLE;
      /*profile_.spi_end = chVTGetSystemTimeX();
      ULOG_INFO("UC1698 (%d bytes): Flush->SPI %lu us, SPI %lu us, Flush total %lu us", pending_flush_.size,
                TIME_I2US(profile_.spi_start - profile_.flush_start), TIME_I2US(profile_.spi_end - profile_.spi_start),
                TIME_I2US(profile_.spi_end - profile_.flush_start));*/
    }
  }
}

}  // namespace xbot::driver::ui
