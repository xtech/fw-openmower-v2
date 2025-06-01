//
// Created by Apehaenger on 5/27/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_DRIVER_S2_HW01_HPP
#define OPENMOWER_SABO_COVER_UI_DRIVER_S2_HW01_HPP

#include "ch.h"
#include "hal.h"
#include "sabo_cover_ui_driver_hw01_base.hpp"
#include "sabo_cover_ui_driver_s2_base.hpp"

namespace xbot::driver::ui {

// Sabo CoverUI Series-II, Hardware v0.1 driver
class SaboCoverUIDriverS2HW01 final : public SaboCoverUIDriverS2Base, public SaboCoverUIDriverHW01Base {
 public:
  explicit SaboCoverUIDriverS2HW01(const DriverConfig& config) : SaboCoverUIDriverBase(config) {
  }
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_DRIVER_S2_HW01_HPP
