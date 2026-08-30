/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sabo_cover_ui_series1_v03.hpp
 * @brief Sabo CoverUI Series1 Driver for Hardware v0.3 with TCA9545 & TCA9535 GPIO expander
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-05-03
 */

#ifndef OPENMOWER_SABO_COVER_UI_SERIES1_V03_HPP
#define OPENMOWER_SABO_COVER_UI_SERIES1_V03_HPP

#include "sabo_cover_ui_series1_gpio.hpp"

namespace xbot::driver::ui {

/** CoverUI Series-I driver for Carrierboard v0.3.
 *
 * Uses GPIO expander with LEDs at bits [0:4] and buttons at bits [0:8].
 */
using SaboCoverUISeries1V03 = SaboCoverUISeries1GPIO<0b00011111, 0>;

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_SERIES1_V03_HPP
