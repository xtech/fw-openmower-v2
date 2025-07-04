//
// Created by Apehaenger on 6/24/25.
//

/**
 * @brief Model AGG240160B05?
 *
 * 240*160 pixels, UC1698u controller
 *
 * The display does support gray-scale mode, and UC1698 is already configured for optimized gray-scale operation,
 * but due to it's bad quality, in fact only 1 or max. 2 gray scales are usabe:
 * 0xf = black, 0xa = 50%, (0x2 = 25%), 0x0 = off
 *
 * PINS:
 * ID0 = 0 => ?
 * ID1 = 0 => 8-bit input data D[0,2,4,6,8,10,12,14]
 * BM[1:0] = 00 => SPI w/ 8-bit token
 * WR[1:0] = NC
 *
 */

#ifndef OPENMOWER_SABO_COVER_UI_DISPLAY_DRIVER_UC1698_HPP
#define OPENMOWER_SABO_COVER_UI_DISPLAY_DRIVER_UC1698_HPP

#include <ch.h>
#include <hal.h>

#include "sabo_cover_ui_types.hpp"
#include "spi_transaction_queue.hpp"

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

  void Start() {
    thread_ = chThdCreateStatic(&wa_, sizeof(wa_), NORMALPRIO, ThreadHelper, this);
#ifdef USE_SEGGER_SYSTEMVIEW
    processing_thread_->name = "SaboCoverUIDisplayDriverUC1698";
#endif
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
    SendCommand(0b11010100 | 0b01);  // [21] Set Color Mode to RRRR-GGGG-BBBB, 4k-color
    SetDisplayEnable(true);
  }

  // [10] Set VBias Potentiometer
  void SetVBiasPotentiometer(uint8_t data) {
    uint8_t cmd[] = {0b10000001, data};
    SendCommands(cmd, sizeof(cmd));
  }

  void SetDisplayEnable(bool on) {
    SendCommand(0b10101000 |
                (on ? 0b111 : 0b110));  // [17] Set Display Enable: Green Enh. Mode ?, Gray Shade On, Active
    display_enabled_ = on;
  }

  bool IsDisplayEnabled() {
    return display_enabled_;
  }

  static void SpiDataCallback(SPIDriver* spip) {
    if (!spiIsBufferComplete(spip)) return;

    chSysLockFromISR();
    auto* drv = SaboCoverUIDisplayDriverUC1698::InstancePtr();
    if (drv && drv->transfer_state_ == TransferState::FLUSH_AREA) {
      chEvtBroadcastI(&drv->flush_area_);
    }
    chSysUnlockFromISR();
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

  /*void FlushArea(const lv_area_t* area, const lv_color_t* color_p) {
    TransferRequest req{.type = TransferState::FLUSH_CB,
                        .x1 = static_cast<uint16_t>(area->x1),
                        .y1 = static_cast<uint16_t>(area->y1),
                        .x2 = static_cast<uint16_t>(area->x2),
                        .y2 = static_cast<uint16_t>(area->y2),
                        .data = reinterpret_cast<const uint8_t*>(color_p),
                        .data_size = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1) * 2};
    transfer_queue_.post(req, TIME_IMMEDIATE);
  }*/

 private:
  THD_WORKING_AREA(wa_, 1024);
  thread_t* thread_ = nullptr;

  LCDCfg lcd_cfg_;
  SPIConfig spi_config_;

  const uint16_t lcd_width_;
  const uint16_t lcd_height_;
  bool display_enabled_;  // Sleep mode of UC1698

  enum class TransferState { SYNC, FLUSH_AREA };
  volatile TransferState transfer_state_ = TransferState::SYNC;

  EVENTSOURCE_DECL(flush_area_);  // FillScreen

  // We do send block wise. This is the number of byte left to send for the next blocks
  uint32_t block_bytes_remaining_ = 0;

  explicit SaboCoverUIDisplayDriverUC1698(LCDCfg lcd_cfg, uint16_t lcd_width, uint16_t lcd_height)
      : lcd_cfg_(lcd_cfg), lcd_width_(lcd_width), lcd_height_(lcd_height) {
    SaboCoverUIDisplayDriverUC1698::InstancePtr() = this;
  }

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
    buf[0] = ((p1 ? 0xf : 0) << 4) | ((p2 ? 0xf : 0));
    buf[1] = ((p3 ? 0xf : 0) << 4) | ((p4 ? 0xf : 0));
    buf[2] = ((p5 ? 0xf : 0) << 4) | ((p6 ? 0xf : 0));
    spiSend(lcd_cfg_.spi.instance, 3, buf);
  }

  /*void StartTransfer() {
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

  void SendNextBlock() {
    uint32_t chunk_size = MIN(block_bytes_remaining_, spi_tx_buf_size_);
    memset(spi_tx_buf_, current_fill_value_, chunk_size);

    spiStartSend(lcd_cfg_.spi.instance, chunk_size, spi_tx_buf_);
    block_bytes_remaining_ -= chunk_size;
  }*/

  /*void HandleTransferCompletion() {
    if (transfer_state_ == TransferState::FILL_SCREEN) {
      SendNextBlock();
    } else {  // FLUSH_CB
      CompleteTransfer();
    }
  }*/

  void CompleteTransfer() {
    transfer_state_ = TransferState::SYNC;
    spiUnselect(lcd_cfg_.spi.instance);
    spiReleaseBus(lcd_cfg_.spi.instance);

    // Notify LVGL if this was a flush operation
    /*if (transfer_state_ == TransferState::FLUSH_CB && flush_ready_cb_) {
      flush_ready_cb_();
    }*/
  }

  static void ThreadHelper(void* instance) {
    auto* i = static_cast<SaboCoverUIDisplayDriverUC1698*>(instance);
    i->ThreadFunc();
  }

  void ThreadFunc() {
    InitController();           // Initialize the LCD controller
    SetVBiasPotentiometer(90);  // TODO: Make Bias configurable?!
    FillScreen(false);          // Clear screen

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
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_DISPLAY_DRIVER_UC1698_HPP
