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

#include "lvgl.h"
#include "sabo_cover_ui_types.hpp"

namespace xbot::driver::ui {

using namespace sabo;

class SaboCoverUIDisplayDriverUC1698 {
 public:
  // Singleton accessor that creates the instance on first call with provided parameters.
  // Usage: SaboCoverUIDisplayDriverUC1698::Instance(cfg, width, height).SendCommand(...);
  // Note: The first call initializes the singleton with the given arguments; subsequent calls ignore them.
  static SaboCoverUIDisplayDriverUC1698& Instance(LCDCfg lcd_cfg) {
    if (!instance_) {
      // Uncommon, but quicker for LVGLFlushCallback()
      static SaboCoverUIDisplayDriverUC1698 instance(lcd_cfg);
      instance_ = &instance;
    }
    return *instance_;
  }

  // Convenience singleton accessor using global/default configuration.
  // Usage: SaboCoverUIDisplayDriverUC1698::Instance().SendCommand(...);
  // Note: Uses global LCDCfg and hardcoded dummy dimensions (they're not used anyway)
  static SaboCoverUIDisplayDriverUC1698& Instance() {
    return Instance(LCDCfg{});  // Dummy values
  }
  // Ptr to Singleton i.e.: auto* drv = SaboCoverUIDisplayDriverBase::InstancePtr();
  static SaboCoverUIDisplayDriverUC1698* InstancePtr() {
    return instance_;
  }

  bool Init();
  void InitController();

  void Start();

  void SetVBiasPotentiometer(uint8_t data);  // [10] Set VBias Potentiometer
  void SetDisplayEnable(bool on);            // [17] Set Display Enable: Green Enh. Mode ?, Gray Shade On, Active

  bool IsDisplayEnabled() {
    return display_enabled_;
  }

  static void SpiDataCallback(SPIDriver* spip);

  void FillScreen(const uint8_t color);

  static void LVGLRounderCallback(lv_event_t* e);
  static void LVGLFlushCallback(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map);

 private:
  static SaboCoverUIDisplayDriverUC1698* instance_;  // Singleton instance and fast LVGL callback access

  THD_WORKING_AREA(wa_, 1024);
  thread_t* thread_ = nullptr;

  LCDCfg lcd_cfg_;
  SPIConfig spi_config_;

  bool display_enabled_;  // Sleep mode of UC1698

  enum class TransferState { SYNC, FLUSH_AREA };
  volatile TransferState transfer_state_ = TransferState::SYNC;

  EVENTSOURCE_DECL(flush_area_);  // FillScreen

  uint32_t block_bytes_remaining_ =
      0;  // We do send block wise. This is the number of byte left to send for the next blocks

  explicit SaboCoverUIDisplayDriverUC1698(LCDCfg lcd_cfg) : lcd_cfg_(lcd_cfg) {
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

  void SendCommand(const uint8_t cmd);
  void SendCommands(const uint8_t* data, size_t size);
  void SendData(const uint8_t* data, size_t size);

  void SetWindowProgramAreaRaw(uint8_t t_x1, uint8_t t_x2, uint8_t t_y1, uint8_t t_y2, bool t_outside_mode = false);
  void SendPixelSextetRaw(const uint8_t* px);
  void Send24MonoPixelsAs12NibblesRaw(uint32_t pix24);

  void CompleteTransfer();

  static void ThreadHelper(void* instance) {
    auto* i = static_cast<SaboCoverUIDisplayDriverUC1698*>(instance);
    i->ThreadFunc();
  }

  void ThreadFunc();
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_DISPLAY_DRIVER_UC1698_HPP
