//
// Created by Apehaenger on 5/27/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_DRIVER_S2_HW01_HPP
#define OPENMOWER_SABO_COVER_UI_DRIVER_S2_HW01_HPP

#include "ch.h"
#include "hal.h"
#include "sabo_cover_ui_driver_base.hpp"

namespace xbot::driver::ui {

// Sabo CoverUI Series-II Hardware v0.1 driver
class SaboCoverUIDriverS2HW01 final : public SaboCoverUIDriverBase {
 public:
  explicit SaboCoverUIDriverS2HW01(const SaboDriverConfig& config) : SaboCoverUIDriverBase(config) {
  }

  bool Init() override;                          // Init SPI and GPIOs
  void LatchLoad() override;                     // Latch LEDs as well as button-row, and load button columns
  void EnableOutput() override;                  // Enable output for HEF4794BT
  uint16_t GetRawButtonStates() const override;  // Get the raw button states (0-15). Low-active!
  void SetLEDs(uint8_t leds) override;           // Set LEDs to a specific pattern
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_DRIVER_S2_HW01_HPP
