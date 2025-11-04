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

#ifndef OPENMOWER_SABO_COVER_UI_DISPLAY_DRIVER_UC1698_HPP
#define OPENMOWER_SABO_COVER_UI_DISPLAY_DRIVER_UC1698_HPP

#include <ch.h>
#include <hal.h>
#include <lvgl.h>

#include "sabo_cover_ui_defs.hpp"

namespace xbot::driver::ui {

using namespace sabo;
using namespace sabo::display;

class SaboCoverUIDisplayDriverUC1698 {
 public:
  static SaboCoverUIDisplayDriverUC1698& Instance(const LCDCfg* lcd_cfg = nullptr) {
    static bool initialized = false;
    chDbgAssert(lcd_cfg != nullptr || initialized, "First call to Instance() must provide LCDCfg!");
    static SaboCoverUIDisplayDriverUC1698 instance_placeholder{lcd_cfg ? *lcd_cfg : LCDCfg{}};
    initialized = true;
    return instance_placeholder;
  }

  enum class TransferState { IDLE, QUEUED, SYNC, FLUSH_AREA };

  bool Init();            // Configure pins and SPI config
  void InitController();  // Initialize UC1698 (needs to be called from thread!)

  void Start();  // Start UC1698 own thread (required for async SPI)

  void SetVBiasPotentiometer(uint8_t data);     // [10] Set VBias Potentiometer (Contrast: 0-255)
  void SetTemperatureCompensation(uint8_t tc);  // [5] Set Temperature Compensation (0-3)
  void SetDisplayEnable(bool on);               // [17] Set Display Enable: Green Enh. Mode off, Gray Shade, Active

  bool IsDisplayEnabled() const {  // LCD active (or sleeping)
    return display_enabled_;
  }

  static void LVGLRounderCB(lv_event_t* e);
  static void LVGLFlushCB(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map);
  static void LVGLFlushWaitCB(lv_display_t* disp);
  static void SPIDataCB(SPIDriver* spip);

 private:
  SaboCoverUIDisplayDriverUC1698(const LCDCfg& lcd_cfg) : lcd_cfg_(lcd_cfg) {
  }

  THD_WORKING_AREA(wa_, 768);  // AH20250714 In use = 512
  thread_t* thread_ = nullptr;

  // We need some signals to start and finish our async SPI transfers
  EVENTSOURCE_DECL(event_source_);

  // With UC1698u RRRR-GGGG-BBBB, 4k-color mode, we send 2 pixels per byte (3 bytes per sextet)
  static constexpr size_t buffer_size_ = LCD_WIDTH * LCD_HEIGHT / BUFFER_FRACTION / 2;
  struct AsyncFlush {
    lv_area_t area;  // Area in buffer
    uint8_t buffer[buffer_size_];
    size_t size = 0;  // Size of buffer
  };
  AsyncFlush pending_flush_;

  LCDCfg lcd_cfg_;
  SPIConfig spi_config_;

  bool display_enabled_ = false;  // Sleep mode of UC1698

  volatile TransferState transfer_state_ = TransferState::IDLE;

  enum class TransferEvent : eventmask_t {
    NONE = 0,
    ASYNC_BUF_READY = EVENT_MASK(0),  // LVGLFlushCB filled spi_tx_buf_ => Start SPI transfer
    ASYNC_TX_DONE = EVENT_MASK(1),    // SPI-Transfer done. spiUnselect, spiRelease, inform lvgl
  };

  // Some benchmarking vars
  /*struct Profile {
    systime_t flush_start, spi_start, spi_end;
  } profile_;*/

  // clang-format off
  // Reset (RST)
  inline void SetRST() { palWriteLine(lcd_cfg_.pins.rst, PAL_HIGH); }
  inline void ClrRST() { palWriteLine(lcd_cfg_.pins.rst, PAL_LOW); }
  // Data/Command (D/C)
  inline void SetDataMode() { palWriteLine(lcd_cfg_.pins.dc, PAL_HIGH); }
  inline void SetCmdMode() { palWriteLine(lcd_cfg_.pins.dc, PAL_LOW); }
  // clang-format on

  void SendCommand(const uint8_t cmd);
  void SendCommands(const uint8_t* data, size_t size);
  void SendData(const uint8_t* data, size_t size);

  void SetWindowProgramAreaRaw(uint8_t t_x1, uint8_t t_x2, uint8_t t_y1, uint8_t t_y2, bool t_outside_mode = false);

  static void ThreadHelper(void* instance) {
    auto* i = static_cast<SaboCoverUIDisplayDriverUC1698*>(instance);
    i->ThreadFunc();
  }

  void ThreadFunc();
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_DISPLAY_DRIVER_UC1698_HPP
