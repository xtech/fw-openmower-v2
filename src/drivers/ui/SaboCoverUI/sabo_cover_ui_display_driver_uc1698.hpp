//
// Created by Apehaenger on 6/24/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_DISPLAY_DRIVER_UC1698_HPP
#define OPENMOWER_SABO_COVER_UI_DISPLAY_DRIVER_UC1698_HPP

#include "sabo_cover_ui_types.hpp"

namespace xbot::driver::ui {

class SaboCoverUIDisplayDriverUC1698 {
 public:
  // Singleton accessor that creates the instance on first call with provided parameters.
  // Usage: SaboCoverUIDisplayDriverUC1698::Instance(cfg, width, height).SendCommand(...);
  // Note: The first call initializes the singleton with the given arguments; subsequent calls ignore them.
  static SaboCoverUIDisplayDriverUC1698& Instance(LCDCfg lcd_cfg, uint16_t lcd_width, uint16_t lcd_height) {
    static SaboCoverUIDisplayDriverUC1698 instance(lcd_cfg, lcd_width, lcd_height);
    return instance;
  }

  // Convenience singleton accessor using global/default configuration.
  // Usage: SaboCoverUIDisplayDriverUC1698::Instance().SendCommand(...);
  // Note: Uses global LCDCfg and hardcoded dummy dimensions (they're not used anyway)
  static SaboCoverUIDisplayDriverUC1698& Instance() {
    return Instance(LCDCfg{}, 100, 100);  // Dummy values
  }
  // Ptr to Singleton i.e.: auto* drv = SaboCoverUIDisplayDriverBase::InstancePtr();
  static SaboCoverUIDisplayDriverUC1698*& InstancePtr() {
    static SaboCoverUIDisplayDriverUC1698* instance = nullptr;
    return instance;
  }

  event_source_t spi_fill_event;  // FillScreen
  // event_source_t spi_flush_event;  // LVGL flush_callback
  //  event_source_t spi_image_event_;

  bool Init() {
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

  void InitController() {
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
  void SetVBiasPotentiometer(uint8_t data) {
    uint8_t cmd[] = {0b10000001, data};
    SendCommands(cmd, sizeof(cmd));
  }

  static void SpiDataCallback(SPIDriver* spip) {
    if (spiIsBufferComplete(spip)) {
      // Get driver instance and call OnSpiTransferDone() handler
      auto* drv = SaboCoverUIDisplayDriverUC1698::InstancePtr();
      if (!drv) return;

      switch (drv->transfer_state_) {
        case TransferState::FILL_SCREEN:
          chSysLockFromISR();
          chEvtBroadcastI(&drv->spi_fill_event);
          chSysUnlockFromISR();
          break;
        case TransferState::IDLE:
          // No action needed, just return
          break;
      }
    } else {
      asm("nop");  // No data received, just return
    }
  }

  void FillScreenFast(bool black, bool first_call = true) {
    if (first_call) {
      if (transfer_state_ != TransferState::IDLE) return;  // Already another transfer running

      // Calc screensize to bytes
      const uint32_t pixels = lcd_width_ * lcd_height_;
      const uint32_t bytes = (pixels / 6) * 3;  // 6 monochrome Pixel = 3 Bytes

      block_bytes_remaining_ = bytes;

      // Fill buffer
      uint8_t val = black ? 0xFF : 0x00;
      for (size_t i = 0; i < spi_tx_buf_size_; ++i) {
        spi_tx_buf_[i] = val;
      }

      spiAcquireBus(lcd_cfg_.spi.instance);
      spiSelect(lcd_cfg_.spi.instance);
      SetCmdMode();
      SetWindowProgramAreaRaw(0, lcd_width_ - 1, 0, lcd_height_ - 1);

      transfer_state_ = TransferState::FILL_SCREEN;
      SetDataMode();
    }

    // Block senden (bei Erstaufruf und bei Folgeaufrufen identisch)
    if (transfer_state_ == TransferState::FILL_SCREEN) {
      if (block_bytes_remaining_ > 0) {
        uint32_t bytes_to_send =
            (block_bytes_remaining_ < spi_tx_buf_size_) ? block_bytes_remaining_ : spi_tx_buf_size_;
        block_bytes_remaining_ -= bytes_to_send;
        spiStartSend(lcd_cfg_.spi.instance, bytes_to_send, spi_tx_buf_);
      } else {
        transfer_state_ = TransferState::IDLE;
        spiUnselect(lcd_cfg_.spi.instance);
        spiReleaseBus(lcd_cfg_.spi.instance);
      }
    }
  }

  void FillScreen(bool black) {
    spiAcquireBus(lcd_cfg_.spi.instance);
    spiStart(lcd_cfg_.spi.instance, &spi_config_);
    spiSelect(lcd_cfg_.spi.instance);

    SetCmdMode();
    SetWindowProgramAreaRaw(0, lcd_width_ - 1, 0, lcd_height_ - 1);

    SetDataMode();
    for (uint8_t i = 0; i < lcd_height_; i++) {
      for (uint8_t j = 0; j < lcd_width_ / 3; j++) {
        SendPixelSextetRaw(black, black, black, black, black, black);
      }
    }

    spiUnselect(lcd_cfg_.spi.instance);
    spiReleaseBus(lcd_cfg_.spi.instance);
  }

 protected:
  LCDCfg lcd_cfg_;
  SPIConfig spi_config_;

  const uint16_t lcd_width_;
  const uint16_t lcd_height_;

  static constexpr size_t spi_tx_buf_size_ = 2048;
  uint8_t spi_tx_buf_[spi_tx_buf_size_];

  enum class TransferState { IDLE, FILL_SCREEN };
  volatile TransferState transfer_state_ = TransferState::IDLE;

  // We do send block wise. This is the number of byte left to send for the next blocks
  uint32_t block_bytes_remaining_ = 0;

  // clang-format off
  // Data/Command (D/C)
  inline void SetDC() { palWriteLine(lcd_cfg_.pins.dc, PAL_HIGH); }
  inline void ClrDC() { palWriteLine(lcd_cfg_.pins.dc, PAL_LOW); }
  // Reset (RST)
  inline void SetRST() { palWriteLine(lcd_cfg_.pins.rst, PAL_HIGH); }
  inline void ClrRST() { palWriteLine(lcd_cfg_.pins.rst, PAL_LOW); }
  // Some comfort shortcuts
  inline void SetCmdMode() { ClrDC(); }
  inline void SetDataMode() { SetDC(); }
  // clang-format on

  void SendCommand(const uint8_t cmd) {
    spiAcquireBus(lcd_cfg_.spi.instance);
    spiStart(lcd_cfg_.spi.instance, &spi_config_);
    spiSelect(lcd_cfg_.spi.instance);
    SetCmdMode();
    spiSend(lcd_cfg_.spi.instance, 1, &cmd);
    spiUnselect(lcd_cfg_.spi.instance);
    spiReleaseBus(lcd_cfg_.spi.instance);
  }

  void SendCommands(const uint8_t* data, size_t size) {
    spiAcquireBus(lcd_cfg_.spi.instance);
    spiStart(lcd_cfg_.spi.instance, &spi_config_);
    spiSelect(lcd_cfg_.spi.instance);
    SetCmdMode();
    spiSend(lcd_cfg_.spi.instance, size, data);
    spiUnselect(lcd_cfg_.spi.instance);
    spiReleaseBus(lcd_cfg_.spi.instance);
  }

  void SendData(const uint8_t* data, size_t size) {
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
    spiSend(lcd_cfg_.spi.instance, sizeof(data) / sizeof(data[0]), data);
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
    spiSend(lcd_cfg_.spi.instance, 3, buf);
  }

 private:
  explicit SaboCoverUIDisplayDriverUC1698(LCDCfg lcd_cfg, uint16_t lcd_width, uint16_t lcd_height)
      : lcd_cfg_(lcd_cfg), lcd_width_(lcd_width), lcd_height_(lcd_height) {
    SaboCoverUIDisplayDriverUC1698::InstancePtr() = this;
    chEvtObjectInit(&spi_fill_event);
    // chEvtObjectInit(&spi_flush_event);
  };
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_DISPLAY_DRIVER_UC1698_HPP
