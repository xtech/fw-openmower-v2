/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file adc3.hpp
 * @brief Minimal ADC3 driver mainly for VREFINT measurements.
 * VREFINT could also get measured from ADC2 but for that ADC12 need to run in DUAL_MODE,
 * which is pretty complicated and has some drawbacks.
 * Due to the ADC12/ADC3 difference, there's no ChbiOS ADC3 implementation, why I created
 * this small driver/wrapper.
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-01-30
 */

#ifndef ADC3_DRIVER_HPP
#define ADC3_DRIVER_HPP

#include <ch.h>
#include <hal.h>
#include <ulog.h>

#include <cstdint>
#include <limits>

namespace xbot::driver::adc3 {

enum class Resolution { BITS_12 = 0, BITS_10, BITS_8, BITS_6 };
enum class SampleRate {
  CYCLES_2P5 = 0,
  CYCLES_6P5,
  CYCLES_12P5,
  CYCLES_24P5,
  CYCLES_47P5,
  CYCLES_92P5,
  CYCLES_247P5,
  CYCLES_640P5
};
enum class OversampleRatio { X2 = 0, X4, X8, X16, X32, X64, X128, X256, NONE };
enum class OversampleShift { NO_SHIFT = 0, SHIFT_1, SHIFT_2, SHIFT_3, SHIFT_4, SHIFT_5, SHIFT_6, SHIFT_7, SHIFT_8 };

/**
 * @brief Initialize ADC3
 */
void Init();

/**
 * @brief Start, enable VReg, Calibrate, ... ADC3
 */
void Start();

/**
 * @brief Start a conversion of a single channel
 *
 * @param chan Channel e.g. ADC_CHANNEL_IN18
 * @param smpr Sample rate
 * @param res  Data resolution, default 12-bits
 * @param ovsr Oversample ratio, default none
 * @param ovss Oversample shift, default no-shift
 */
void StartConvert(adc_channels_num_t chan, SampleRate smpr, Resolution res = Resolution::BITS_12,
                  OversampleRatio ovsr = OversampleRatio::NONE, OversampleShift ovss = OversampleShift::NO_SHIFT);

/**
 * @brief Wait for Conversion end and return raw values
 *
 * @return uint16_t
 */
uint16_t WaitForConversion();

/**
 * @brief Calculate actual VREF voltage using calibration
 * @param raw_value Raw ADC reading
 * @return Actual VREF voltage in volts
 */
float GetVrefVoltage(uint16_t raw_value);

/**
 * @brief Acquire ADC3 mutex for exclusive access
 * @note Must be called before using ADC3
 */
void Acquire();

/**
 * @brief Release ADC3 mutex
 * @note Must be called after finishing with ADC3
 */
void Release();

}  // namespace xbot::driver::adc3

#endif  // ADC3_DRIVER_HPP
