//
// Created by Apehaenger on 6/24/25.
//

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
 *
 */

#include "sabo_cover_ui_display_driver_uc1698.hpp"

#include "sabo_cover_ui_display.hpp"

namespace xbot::driver::ui {

// Static instance_ initialization
SaboCoverUIDisplayDriverUC1698* SaboCoverUIDisplayDriverUC1698::instance_ = nullptr;

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
      .data_cb = SpiDataCallback,
      .error_cb = NULL,
      .ssline = lcd_cfg_.spi.pins.cs,  // Chip Select line
                                       // UC1698u is rated with a max frequency of 25 MHz
      .cfg1 = SPI_CFG1_MBR_1 |         // Baudrate = FPCLK/8 (25 MHz @ 200 MHz PLL2_P)
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
  thread_ = chThdCreateStatic(&wa_, sizeof(wa_), NORMALPRIO, ThreadHelper, this);
#ifdef USE_SEGGER_SYSTEMVIEW
  processing_thread_->name = "SaboCoverUIDisplayDriverUC1698";
#endif
}

void SaboCoverUIDisplayDriverUC1698::InitController() {
  // Directly after reset set Booster, Regulator, and Power Control in that order
  SendCommand(0b00101000 | 0b0);  // [6] Set Power Control, Booster: LCD<=13nF
  // SendCommand(0b00101000 | 0b1);  // [6] Set Power Control, Booster: 13nF<LCD<=22nF
  chThdSleepMilliseconds(10);
  SendCommand(0b00101000 | 0b10);  // [6] Set Power Control, VReg: Internal Vlcd(*10)
  chThdSleepMilliseconds(100);     // Give charge pump some time to stabilize
  // SendCommand(0b00100100 | 0b01);  // [5] Set Temperature Compensation (-0.05%/C)
  SendCommand(0b00100100 | 0b10);  // [5] Set Temperature Compensation (-0.15%/C)
  // SendCommand(0b00100100 | 0b11);  // [5] Set Temperature Compensation (-0.25%/C)
  SendCommand(0b11000000 | 0b100);  // [18] Set LCD Mapping Control, Mirror Y
  // SendCommand(0b10100110 | 1);     // [16] Set Inverse Display. Will save inversion logic in LVGL's flush_cb()
  SendCommand(0b11010000 | 1);     // [20] Set Color Pattern to R-G-B
  SendCommand(0b11010100 | 0b01);  // [21] Set Color Mode to RRRR-GGGG-BBBB, 4k-color
  // SendCommand(0b11010100 | 0b10);  // [21] Set Color Mode to RRRRR-GGGGGG-BBBBB, 64k-color
  SetDisplayEnable(true);
}

// [10] Set VBias Potentiometer
void SaboCoverUIDisplayDriverUC1698::SetVBiasPotentiometer(uint8_t data) {
  uint8_t cmd[] = {0b10000001, data};
  SendCommands(cmd, sizeof(cmd));
}

void SaboCoverUIDisplayDriverUC1698::SetDisplayEnable(bool on) {
  // [17] Set Display Enable: Green Enh. Mode disabled, Gray Shade On, $on
  SendCommand(0b10101000 | (on ? 0b111 : 0b110));
  display_enabled_ = on;
}

void SaboCoverUIDisplayDriverUC1698::SpiDataCallback(SPIDriver* spip) {
  if (!spiIsBufferComplete(spip)) return;

  chSysLockFromISR();
  auto* drv = SaboCoverUIDisplayDriverUC1698::InstancePtr();
  if (drv && drv->transfer_state_ == TransferState::FLUSH_AREA) {
    chEvtBroadcastI(&drv->flush_area_);
  }
  chSysUnlockFromISR();
}

void SaboCoverUIDisplayDriverUC1698::FillScreen(const uint8_t color) {
  uint8_t sextet[6] = {color, color, color, color, color, color};

  spiAcquireBus(lcd_cfg_.spi.instance);
  spiStart(lcd_cfg_.spi.instance, &spi_config_);
  spiSelect(lcd_cfg_.spi.instance);

  SetCmdMode();
  SetWindowProgramAreaRaw(0, SABO_LCD_WIDTH - 1, 0, SABO_LCD_HEIGHT - 1);

  SetDataMode();
  for (uint8_t i = 0; i < SABO_LCD_HEIGHT; i++) {
    for (uint8_t j = 0; j < SABO_LCD_WIDTH / 3; j++) {
      SendPixelSextetRaw(sextet);
    }
  }

  spiUnselect(lcd_cfg_.spi.instance);
  spiReleaseBus(lcd_cfg_.spi.instance);
}

void SaboCoverUIDisplayDriverUC1698::LVGLRounderCallback(lv_event_t* e) {
  lv_area_t* area = lv_event_get_invalidated_area(e);

  area->x1 = (area->x1 & ~0x7);  // Round down to the nearest multiple of 8
  area->x2 = (area->x2 | 0x7);   // Round up to the next multiple of 8

  // Due to the 25% more efficient RRRR-GGGG-BBBB, 4K-color mode of UC1698,
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

void SaboCoverUIDisplayDriverUC1698::LVGLFlushCallback(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
  if (!instance_) {
    lv_display_flush_ready(disp);
    return;
  }

  // Setze das Fenster für den zu beschreibenden Bereich
  spiAcquireBus(instance_->lcd_cfg_.spi.instance);
  spiStart(instance_->lcd_cfg_.spi.instance, &instance_->spi_config_);
  spiSelect(instance_->lcd_cfg_.spi.instance);

  instance_->SetCmdMode();
  instance_->SetWindowProgramAreaRaw(area->x1, area->x2, area->y1, area->y2);

  instance_->SetDataMode();

  int width = area->x2 - area->x1 + 1;
  int height = area->y2 - area->y1 + 1;
  int total_pixels = width * height;

  const uint8_t* src = px_map + 8;  // Skip 2*4 byte palette
  int blocks = total_pixels / 24;

  for (int b = 0; b < blocks; b++) {
    // Buid 24 Bits from 3 Bytes
    // ATTENTION: Each byte's MSB is the leftmost pixel, which is contrary to the current LVGL 9.3 docs!
    uint32_t pix24 = (src[0] << 16) | (src[1] << 8) | src[2];
    /*uint32_t pix24 = src[0];
    src++;
    pix24 |= (src[0] << 8);
    src++;
    pix24 |= (src[0] << 16);
    src++;*/
    instance_->Send24MonoPixelsAs12NibblesRaw(pix24);
    src += 3;
  }

  spiUnselect(instance_->lcd_cfg_.spi.instance);
  spiReleaseBus(instance_->lcd_cfg_.spi.instance);

  lv_display_flush_ready(disp);
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
  spiSend(lcd_cfg_.spi.instance, sizeof(data) / sizeof(data[0]), data);
}

// This is required for RRRR-GGGG-BBBB, 4K-color mode which need 25% less data than RRRRR-GGGGG-BBBBB, 64K-color mode.
// See UC1698u datasheet: (21) SET COLOR MODE
void SaboCoverUIDisplayDriverUC1698::Send24MonoPixelsAs12NibblesRaw(uint32_t pix24) {
  // MSB = Leftmost Pixel, LSB = Rightmost pixel
  for (int i = 23; i >= 0; i -= 2) {
    bool p1 = (pix24 >> i) & 0x01;
    bool p2 = (pix24 >> (i - 1)) & 0x01;

    const uint8_t data = (p1 ? 0x00 : 0xF0) | (p2 ? 0x00 : 0x0F);  // LVGL bit true == white == LCD dot off

    spiSend(lcd_cfg_.spi.instance, 1, &data);
  }
}

// Send Pixel Sextet will send 2*3 pixels in one command.
// This is required for RRRR-GGGG-BBBB, 4K-color mode which need 25% less data than RRRRR-GGGGG-BBBBB, 64K-color mode.
void SaboCoverUIDisplayDriverUC1698::SendPixelSextetRaw(const uint8_t* px) {
  // Map LVGL grayscale to our usable 4 values
  auto map = [](uint8_t val) -> uint8_t {
    if (val <= 60)
      return 0xff;  // Black
    else if (val <= 140)
      return 0x0a;  // 50% Gray
    else if (val <= 220)
      return 0x05;  // 25% Gray
    else
      return 0x00;  // White/Off
  };

  uint8_t buf[3];
  buf[0] = (map(px[0]) << 4) | map(px[1]);
  buf[1] = (map(px[2]) << 4) | map(px[3]);
  buf[2] = (map(px[4]) << 4) | map(px[5]);

  spiSend(lcd_cfg_.spi.instance, 3, buf);
}

/*void SaboCoverUIDisplayDriverUC1698::StartTransfer() {
  spiAcquireBus(lcd_cfg_.spi.instance);
  spiSelect(lcd_cfg_.spi.instance);

  SetCmdMode();
  SetWindowProgramAreaRaw(req.x1, req.x2, req.y1, req.y2);
  SetDataMode();

  transfer_state_ = req.type;

  // Handle different transfer types
  if (req.type == TransferState::FILL_SCREEN) {
    block_bytes_remaining_ = req.data_size;
    SendNextBlock();
  } else {  // FLUSH_CB
    // Directly send LVGL's buffer in one go
    spiStartSend(lcd_cfg_.spi.instance, req.data_size, req.data);
    // No need to track remaining bytes for flushes
  }
}

void SaboCoverUIDisplayDriverUC1698::SendNextBlock() {
  uint32_t chunk_size = MIN(block_bytes_remaining_, spi_tx_buf_size_);
  memset(spi_tx_buf_, current_fill_value_, chunk_size);

  spiStartSend(lcd_cfg_.spi.instance, chunk_size, spi_tx_buf_);
  block_bytes_remaining_ -= chunk_size;
}*/

/*void SaboCoverUIDisplayDriverUC1698::HandleTransferCompletion() {
  if (transfer_state_ == TransferState::FILL_SCREEN) {
    SendNextBlock();
  } else {  // FLUSH_CB
    CompleteTransfer();
  }
}*/

void SaboCoverUIDisplayDriverUC1698::CompleteTransfer() {
  transfer_state_ = TransferState::SYNC;
  spiUnselect(lcd_cfg_.spi.instance);
  spiReleaseBus(lcd_cfg_.spi.instance);

  // Notify LVGL if this was a flush operation
  /*if (transfer_state_ == TransferState::FLUSH_CB && flush_ready_cb_) {
    flush_ready_cb_();
  }*/
}

void SaboCoverUIDisplayDriverUC1698::ThreadFunc() {
  InitController();            // Initialize the LCD controller
  SetVBiasPotentiometer(100);  // TODO: Make Bias configurable?!
  // FillScreen(0xff);           // Clear screen (color white)

  while (true) {
    // Handle SPI completion events
    /*eventflags_t flags = chEvtGetAndClearFlags(&flush_area_);
    if (flags) {
      //HandleTransferCompletion();
    }*/

    // Sleep max. 10ms for smooth LVGL flush-buffer updates
    chThdSleep(TIME_MS2I(10));
  }
}

}  // namespace xbot::driver::ui
