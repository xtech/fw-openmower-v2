/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file adc1.hpp
 * @brief ADC1 driver with hardware-independent configuration
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-01-25
 */

#ifndef ADC1_DRIVER_HPP
#define ADC1_DRIVER_HPP

#include <etl/array_view.h>
#include <etl/delegate.h>
#include <hal.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace xbot::driver::adc1 {

/**
 * @brief Conversion identifier.
 *
 * Add new CG's as you like (your robot may have)
 */
enum class Adc1ConversionId : uint8_t { V_CHARGER = 0, V_BATTERY, I_IN_DCDC, _NUM_CHANNEL_IDS };

enum class Resolution { BITS_16 = 0, BITS_14, BITS_12, BITS_10, BITS_8 };

struct ResolutionInfo {
  uint8_t bits;
  uint32_t adc_mask;
  uint32_t max_value;
  float max_value_f;

  // Constructor
  constexpr ResolutionInfo(uint8_t b, uint32_t m)
      : bits(b), adc_mask(m), max_value((1U << b) - 1), max_value_f(static_cast<float>((1U << b) - 1)) {
  }

  // Dynamic Scale-Factor calculation
  constexpr float GetScaleFactor(float vref_voltage) const {
    return vref_voltage / max_value_f;
  }

  // Raw to Voltage
  constexpr float RawToVoltage(adcsample_t raw, float vref_voltage) const {
    return static_cast<float>(raw) * GetScaleFactor(vref_voltage);
  }
};

// Compile-time lookup table
static constexpr ResolutionInfo kResolutionInfo[] = {{16, ADC_CFGR_RES_16BITS},
                                                     {14, ADC_CFGR_RES_14BITS},
                                                     {12, ADC_CFGR_RES_12BITS},
                                                     {10, ADC_CFGR_RES_10BITS},
                                                     {8, ADC_CFGR_RES_8BITS}};

constexpr const ResolutionInfo& GetResolutionInfo(Resolution res) {
  return kResolutionInfo[static_cast<uint8_t>(res)];
}

/**
 * @brief Adaptive EMA filter configuration.
 *
 * When enabled, the ADC conversion result is smoothed by an exponential moving average.
 * - alpha: smoothing factor in normal mode (lower = more smoothing, e.g. 0.3 → ~3s time constant)
 * - alpha_fast: smoothing factor when |raw − ema| > threshold (higher = faster tracking, e.g. 0.7)
 * - threshold: deviation in volts that triggers fast tracking (e.g. 0.3V for motor load detection)
 */
struct EmaFilterConfig {
  float alpha = 0.3f;
  float alpha_fast = 0.7f;
  float threshold = 0.5f;
  bool enabled = false;
};

/**
 * @brief ADC sensor configuration and calculation
 */
struct Adc1Sensor {
  adc_channels_num_t channel;  // ADC channel number, see ADC_CHANNEL_IN*
  uint8_t sample_rate;         // Sample rate for this channel, see ADC_SMPR_SMP_*
};

/**
 * @brief ADC1 conversion group definition
 *
 * Contains configuration for an entire ADC conversion group, including
 * all sensors/channels, sampling settings, resolution and buffer configuration.
 */
struct Adc1ConversionGroup {
  Adc1ConversionId id;                        // Unique ID for this conversion (group)
  etl::array_view<const Adc1Sensor> sensors;  // Array of sensor/channel configurations
  adcsample_t* sample_buffer;                 // Sample buffer where ADC will DMA results
  Resolution resolution;                      // ADC resolution
  uint16_t ovs_ratio;                         // Oversample ratio
  uint8_t ovs_rshift;                         // Oversample right-shift

  // Conversion function from raw samples to physical value
  etl::delegate<float(const Adc1ConversionGroup*, float vref_voltage)> convert;
  const void* user_data = nullptr;

  // EMA filter configuration (optional, disabled by default)
  EmaFilterConfig ema_config{};
  float ema_value = std::numeric_limits<float>::quiet_NaN();

  float cached_value{};
  systime_t last_conversion_time{};

  /**
   * @brief Validate configuration
   * @return true if configuration is valid
   */
  constexpr bool IsValid() const {
    return sensors.size() > 0 && sensors.size() <= 20;  // ADC1 has max. 20 channels
  }

  bool IsCacheValid(uint16_t max_age_ms) const {
    if (max_age_ms == 0) return false;
    systime_t now = chVTGetSystemTimeX();
    return (now - last_conversion_time) <= TIME_MS2I(max_age_ms);
  }

  void UpdateCache(float value) {
    last_conversion_time = chVTGetSystemTimeX();
    cached_value = value;
  }

  /**
   * @brief Convert raw ADC sample to voltage
   * @return Voltage in volts
   */
  float RawToVoltage(adcsample_t raw, float vref) const {
    const auto& info = GetResolutionInfo(resolution);
    return info.RawToVoltage(raw, vref);
  }

  /**
   * @brief Apply EMA filter to the converted value (if enabled)
   * @param value Raw converted value
   * @return Filtered value (or raw if EMA disabled)
   */
  float ApplyEma(float value) {
    if (!ema_config.enabled) return value;
    if (std::isnan(ema_value)) {
      ema_value = value;
      return value;
    }
    float alpha = (std::fabs(value - ema_value) > ema_config.threshold) ? ema_config.alpha_fast : ema_config.alpha;
    ema_value = alpha * value + (1.0f - alpha) * ema_value;
    return ema_value;
  }

  // Factory Method
  template <typename F>
  static Adc1ConversionGroup Create(Adc1ConversionId id, etl::array_view<const Adc1Sensor> sensors, adcsample_t* buffer,
                                    Resolution res, uint16_t ovs_ratio, uint8_t ovs_rshift, F&& convert_func,
                                    const void* user_data = nullptr, const EmaFilterConfig& ema = EmaFilterConfig{}) {
    etl::delegate<float(const Adc1ConversionGroup*, float)> delegate;
    delegate = std::forward<F>(convert_func);
    return Adc1ConversionGroup{.id = id,
                               .sensors = sensors,
                               .sample_buffer = buffer,
                               .resolution = res,
                               .ovs_ratio = ovs_ratio,
                               .ovs_rshift = ovs_rshift,
                               .convert = delegate,
                               .user_data = user_data,
                               .ema_config = ema};
  }
};

/**
 * @brief Initialize ADC's
 */
void Init();

/**
 * @brief Start ADC circuits
 * @return true if start successful
 */
bool Start();

/**
 * @brief Synchronous (blocking) ADC convert
 * Get normally called via GetValueOrNaN()
 *
 * @param cg
 * @return true
 * @return false
 */
bool Convert(Adc1ConversionGroup* cg);

/**
 * @brief Get ADC value or NaN object
 * This will return the final calculated ADC value for the given conversion ID,
 * or NaN for the case that the conversion doesn't exists (got registered) for this robot.
 * If the last ADC conversion is older than the given age, a new (blocking) conversion will be made.
 * If an EMA filter is configured for this conversion group, the value will be filtered
 * before caching and returning.
 *
 * @param conv_id
 * @param max_age_ms
 * @return float Value of the calculated ADC result or NaN in the case of a failure or a non registered conversion
 * ID
 */
float GetValueOrNaN(Adc1ConversionId conv_id, uint16_t max_age_ms = 20);

/**
 * @brief Register conversion group to ADC1 by his ID
 *
 * @param cg
 * @return true
 * @return false
 */
bool RegisterConversionGroup(const Adc1ConversionGroup& cg);

Adc1ConversionGroup* GetConversionGroup(Adc1ConversionId id);

void DumpBenchmarkMeasurement(Adc1ConversionId conv_id, const char* sensor_name = "UNDEF", size_t num_samples = 20,
                              bool verbose = true);

}  // namespace xbot::driver::adc1

#endif  // ADC_DRIVER_HPP
