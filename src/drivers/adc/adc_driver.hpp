/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ADC_DRIVER_HPP
#define ADC_DRIVER_HPP

#include <ch.h>
#include <etl/delegate.h>
#include <hal.h>

#include <cstddef>
#include <cstdint>

namespace xbot::driver::adc {

/**
 * @brief Universal ADC driver with DMA support
 *
 * This driver provides a generic interface for ADC conversions using DMA.
 * It is designed to work with the DmaResourceManager for DMA stream management,
 * but does not depend on it.
 *
 * The driver supports:
 * - Single or multiple channels (scan mode)
 * - Linear buffer conversions with DMA
 * - Configurable sampling times
 * - Both single-shot and continuous conversion modes
 * - Voltage conversion with configurable reference voltage
 */
class AdcDriver {
 public:
  /**
   * @brief Configuration structure for the ADC driver
   */
  struct Config {
    ADCDriver* adc;          ///< ADC peripheral (e.g., &ADCD1)
    uint32_t* channels;      ///< Array of ADC channel numbers
    uint8_t num_channels;    ///< Number of channels in the array
    adcsample_t* buffer;     ///< DMA buffer for samples
    size_t buffer_depth;     ///< Number of samples per channel
    uint32_t sampling_time;  ///< Sampling time for all channels (ADC_SMPR_SMP_*)
    bool circular = false;   ///< Circular DMA mode (continuous)
    float vref = 3.3f;       ///< Reference voltage for conversion to volts

    /**
     * @brief Validate configuration
     * @return true if configuration is valid, false otherwise
     */
    bool IsValid() const {
      return adc != nullptr && channels != nullptr && num_channels > 0 && buffer != nullptr && buffer_depth > 0 &&
             sampling_time <= ADC_SMPR_SMP_64P5;
    }
  };

  /**
   * @brief State of the ADC driver
   */
  enum class State {
    STOPPED,     ///< ADC not initialized
    READY,       ///< ADC initialized and ready for conversion
    CONVERTING,  ///< Conversion in progress
    ERROR        ///< Error state
  };

  AdcDriver() = default;
  ~AdcDriver();

  /**
   * @brief Initialize the ADC driver with given configuration
   * @param config Configuration structure
   * @return true if initialization successful, false otherwise
   */
  bool Init(const Config& config);

  /**
   * @brief Start the ADC driver (register with DmaResourceManager if needed)
   *
   * This method should be called after Init() and before any conversions.
   * If using DmaResourceManager, register this driver with the delegates
   * returned by GetStartDelegate() and GetStopDelegate().
   *
   * @return true if start successful, false otherwise
   */
  bool Start();

  /**
   * @brief Stop the ADC driver
   *
   * Stops any ongoing conversion and releases resources.
   */
  void Stop();

  /**
   * @brief Start a single conversion
   *
   * Starts a single conversion sequence. The conversion runs in the background
   * using DMA. Use WaitConversion() to wait for completion.
   *
   * @return true if conversion started, false otherwise
   */
  bool StartConversion();

  /**
   * @brief Start continuous conversions
   *
   * Starts continuous conversions in circular buffer mode.
   * The driver will continuously fill the buffer.
   */
  bool StartContinuous();

  /**
   * @brief Stop ongoing conversions
   */
  void StopConversion();

  /**
   * @brief Wait for conversion completion
   * @param timeout Timeout in milliseconds (0 = wait forever)
   * @return true if conversion completed, false on timeout or error
   */
  bool WaitConversion(systime_t timeout = TIME_INFINITE);

  /**
   * @brief Get the current state of the driver
   */
  State GetState() const {
    return state_;
  }

  /**
   * @brief Get the raw sample buffer
   * @return Pointer to the sample buffer
   */
  adcsample_t* GetBuffer() {
    return config_.buffer;
  }
  const adcsample_t* GetBuffer() const {
    return config_.buffer;
  }

  /**
   * @brief Get the number of channels
   */
  uint8_t GetNumChannels() const {
    return config_.num_channels;
  }

  /**
   * @brief Get the buffer depth (samples per channel)
   */
  size_t GetBufferDepth() const {
    return config_.buffer_depth;
  }

  /**
   * @brief Convert raw sample to voltage
   * @param sample Raw ADC sample (0-4095 for 12-bit)
   * @return Voltage in volts
   */
  float RawToVoltage(adcsample_t sample) const;

  /**
   * @brief Get average voltage for a channel
   * @param channel Channel index (0 to num_channels-1)
   * @return Average voltage in volts, or 0.0f on error
   */
  float GetChannelVoltage(uint8_t channel) const;

  /**
   * @brief Get the latest sample for a channel
   * @param channel Channel index (0 to num_channels-1)
   * @return Latest raw sample, or 0 on error
   */
  adcsample_t GetChannelSample(uint8_t channel) const;

  /**
   * @brief Get delegate for starting the ADC (for DmaResourceManager)
   *
   * This delegate can be used with DmaResourceManager::Register().
   * It calls the internal StartADC() method.
   */
  etl::delegate<msg_t()> GetStartDelegate() {
    return etl::delegate<msg_t()>::create<AdcDriver, &AdcDriver::StartADC>(*this);
  }

  /**
   * @brief Get delegate for stopping the ADC (for DmaResourceManager)
   *
   * This delegate can be used with DmaResourceManager::Register().
   * It calls the internal StopADC() method.
   */
  etl::delegate<void()> GetStopDelegate() {
    return etl::delegate<void()>::create<AdcDriver, &AdcDriver::StopADC>(*this);
  }

  /**
   * @brief Get the ADC peripheral pointer (for resource ID)
   */
  void* GetAdcPeripheral() const {
    return static_cast<void*>(config_.adc);
  }

 private:
  Config config_{};
  State state_ = State::STOPPED;
  ADCConversionGroup conv_group_{};
  semaphore_t conversion_sem_;

  /**
   * @brief Internal method to start ADC (called via delegate)
   * @return HAL_RET_SUCCESS on success, error code otherwise
   */
  msg_t StartADC();

  /**
   * @brief Internal method to stop ADC (called via delegate)
   */
  void StopADC();

  /**
   * @brief Static callback for ADC conversion end
   * @param adcp Pointer to the ADC driver that triggered the callback
   */
  static void ConversionEndCallback(ADCDriver* adcp);

  /**
   * @brief Static callback for ADC error
   * @param adcp Pointer to the ADC driver that triggered the callback
   * @param err Error code
   */
  static void ConversionErrorCallback(ADCDriver* adcp, adcerror_t err);

  // Single instance pointer for callback (assuming only one ADC driver instance)
  static AdcDriver* s_instance_;

  // Disallow copying
  AdcDriver(const AdcDriver&) = delete;
  AdcDriver& operator=(const AdcDriver&) = delete;
};

}  // namespace xbot::driver::adc

#endif  // ADC_DRIVER_HPP
