//
// Created by Apehaenger on 5/26/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_DRIVER_BASE_HPP
#define OPENMOWER_SABO_COVER_UI_DRIVER_BASE_HPP

#include "ch.h"
#include "hal.h"

namespace xbot::driver::ui {

struct SaboDriverConfig {
  SPIDriver* spi_instance;  // SPI-Instance like &SPID1
  struct {
    ioline_t sck;   // Clock (SCK/CLK)
    ioline_t miso;  // Master In Slave Out (MISO/RX)
    ioline_t mosi;  // Master Out Slave In (MOSI/TX)
  } spi_pins;

  struct {
    ioline_t latch_load;
    ioline_t oe;
    ioline_t btn_cs;  // Button Chip Select (74HC165 /CE) only avail for Series-II CoverUI
    ioline_t inp_cs;  // Input Chip Select (74HC165 /CE) only avail in HW >= v0.2
  } control_pins;
};

// Base class for CoverUI Series-I/II
class SaboCoverUIDriverBase {
 protected:
  SaboDriverConfig config_;
  SPIConfig spi_cfg_;

  uint8_t current_leds_ = 0;
  uint8_t current_button_row_ = 0;  // Alternating button rows
  uint16_t button_states_ = 0;      // Bits 0-7: Row0, Bits 8-15: Row1. Low-active!

 public:
  explicit SaboCoverUIDriverBase(const SaboDriverConfig& config) : config_(config) {
    palSetLineMode(config_.control_pins.latch_load,
                   PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_FLOATING);
    palSetLineMode(config_.control_pins.oe,
                   PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_FLOATING);
    if (config_.control_pins.btn_cs != PAL_NOLINE) {
      palSetLineMode(config_.control_pins.btn_cs,
                     PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_FLOATING);
    }
  }

  virtual ~SaboCoverUIDriverBase() = default;

  virtual bool Init();                              // Init SPI
  virtual void LatchLoad() = 0;                     // Latch LEDs as well as button-row, and load button columns
  virtual void EnableOutput() = 0;                  // Enable output for HEF4794BT
  virtual uint16_t GetRawButtonStates() const = 0;  // Get the raw button states (0-15). Low-active!
  virtual void SetLEDs(uint8_t leds) = 0;           // Set LEDs to a specific pattern
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_DRIVER_BASE_HPP
