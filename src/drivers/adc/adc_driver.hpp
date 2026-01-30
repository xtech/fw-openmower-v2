/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file adc_driver.hpp
 * @brief ADC driver with hardware-independent configuration
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-01-25
 */

#ifndef ADC_DRIVER_HPP
#define ADC_DRIVER_HPP

#include <etl/array_view.h>
#include <etl/delegate.h>
#include <hal.h>

#include <cstddef>
#include <cstdint>

namespace xbot::driver::adc {

// ChannelId for ADC channel identification
// Add new channels as you like (your robot has)
enum class ChannelId : uint8_t { V_CHARGER = 0, V_BATTERY, I_IN_DCDC, T_BATTERY, T_MCU, _NUM_CHANNEL_IDS };

/**
 * @brief ADC channel configuration and calculation
 */
struct AdcChannel {
  ChannelId id;         // Unique ID for this channel
  uint8_t channel;      // ADC channel number, see ADC_CHANNEL_IN*
  uint8_t sample_rate;  // Sample rate for this channel, see ADC_SMPR_SMP_*

  // Conversion function from raw sample to physical value
  etl::delegate<float(adcsample_t, const void*)> convert;
  const void* user_data = nullptr;

  // Some usefull 16-bit ADC constants for raw to V/A/... conversions
  static constexpr float VREF = 3.33f;
  static constexpr uint8_t BITS = 16;
  static constexpr uint32_t MAX_VALUE = (1 << BITS) - 1;  // 65535 @ 16bit resolution
  static constexpr float MAX_VALUE_F = static_cast<float>(MAX_VALUE);
  static constexpr float SCALE_FACTOR = VREF / MAX_VALUE_F;

  /**
   * @brief Convert raw ADC sample to voltage
   * @return Voltage in volts
   */
  static constexpr float RawToVoltage(adcsample_t raw) {
    return static_cast<float>(raw) * SCALE_FACTOR;
  }

  // Factory Method
  template <typename F>
  static AdcChannel Create(ChannelId id, uint8_t channel, uint8_t sample_rate, F&& func, const void* user_data) {
    etl::delegate<float(adcsample_t, const void*)> delegate;
    delegate = std::forward<F>(func);
    return AdcChannel{
        .id = id, .channel = channel, .sample_rate = sample_rate, .convert = delegate, .user_data = user_data};
  }
};

/**
 * @brief Complete ADC configuration for a hardware instance
 *
 * Contains configuration for an entire ADC peripheral including
 * all channels, sampling settings, and buffer configuration.
 */
struct AdcConfig {
  ADCDriver* drv;                              // ADC driver (e.g., &ADCD1)
  etl::array_view<const AdcChannel> channels;  // Array of channel configurations
  adcsample_t* sample_buffer;                  // Sample buffer where ADC will DMA results

  /**
   * @brief Validate configuration
   * @return true if configuration is valid
   */
  constexpr bool IsValid() const {
    return drv != nullptr && channels.size() > 0 && channels.size() <= 20;
  }
};

/**
 * @brief Simple ADC driver for hardware-independent measurements
 *
 * Driver around ChibiOS ADC API that uses AdcConfig for configuration.
 *
 * ATTENTION: Don't use this driver if you require quick and high frequent conversions,
 * because of somehow uncommon driver implementation, which:
 * - Boundles all your ADC channels into one configuration group.
 *   See CreateAdcConfig() in one of the robot implementaions,how to do that.
 * - Convert() might get called, but don't need to! Instead:
 * - Directly ask for an ADC value via `AdcDriver::GetChannelValue(ChannelId::V_BATTERY, 100)`.
 *   Take note about the 2nd `max_age_ms` argument, which will trigger driver internally an sync adcCovert()
 *   of the whole ConversionGroup if the last converted ADC samples are older.
 */
class AdcDriver {
 public:
  /**
   * @brief Initialize ADC with given configuration
   * @param config ADC configuration
   * @return true if initialization successful
   */
  bool Init(const AdcConfig& config);

  /**
   * @brief Start ADC conversions
   * @return true if start successful
   */
  bool Start();

  /**
   * @brief Stop ADC conversions
   */
  void Stop();

  /**
   * @brief Do a synchronous (blocking) adcConvert()
   * @return MSG_OK if conversion successful
   */
  msg_t Convert();

  /**
   * @brief   Get the raw ADC sample of a given channel.
   * @details Performs a synchronous conversion operation if last conversion is older than max_age_ms.
   * @param[in] channel_id   Channel ID to read
   * @param[in] max_age_ms   Maximum age of the conversion in milliseconds.
   *                         Will trigger a new conversion is last one is older
   * @return                 ADC raw sample
   */
  adcsample_t GetChannelRawValue(ChannelId channel_id, uint16_t max_age_ms = 20);

  /**
   * @brief   Get the final (converted) value for an ADC sample of the given channel.
   * @details Performs a synchronous conversion operation if last conversion is older than max_age_ms.
   *          If an AdcChannel.convert (lambda) function exists, that result get
   *          Applies the configured AdcChannel.convert function(lambda) or convert
   * @param[in] channel_id   Channel ID to read
   * @param[in] max_age_ms   Maximum age of the conversion in milliseconds.
   *                         Will trigger a new conversion is last one is older
   * @return                 AdcChannel.convert (lambda) or if AdcChannel.convert don't exists,
   *                         AdcChannel.RawToVoltage() result
   */
  float GetChannelValue(ChannelId channel_id, uint16_t max_age_ms = 20);

  /**
   * @brief Get the configuration
   */
  const AdcConfig& GetConfig() const {
    return config_;
  }

 private:
  AdcConfig config_{};
  ADCConversionGroup conv_group_{};

  bool initialized_ = false;
  systime_t last_conversion_{};

  // Lookup: ChannelId -> channel index (or -1 if not present)
  int8_t channel_id_to_index_[static_cast<uint8_t>(ChannelId::_NUM_CHANNEL_IDS)];

  // Constructor
  AdcDriver() {
    // Initialize channel ID to index map with -1 (not present)
    for (size_t i = 0; i < static_cast<uint8_t>(ChannelId::_NUM_CHANNEL_IDS); ++i) channel_id_to_index_[i] = -1;
  }

  friend bool InitAdcDriver(const AdcConfig& config);

  /**
   * @brief Do conversion if last conversion is older than max_age_ms ago
   * @param max_age_ms Maximum age of last conversion in milliseconds
   * @return MSG_OK if conversion successful
   */
  void ConvertIfOutdated(uint16_t max_age_ms);

  // Disallow copying
  AdcDriver(const AdcDriver&) = delete;
  AdcDriver& operator=(const AdcDriver&) = delete;
};

// Singleton accessor
AdcDriver* GetAdcDriver();
bool InitAdcDriver(const AdcConfig& config);
void DeInitAdcDriver();

}  // namespace xbot::driver::adc

#endif  // ADC_DRIVER_HPP
