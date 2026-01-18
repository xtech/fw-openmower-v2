/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "adc_driver.hpp"

#include <ch.h>
#include <hal.h>
#include <ulog.h>

namespace xbot::driver::adc {

// Static member definitions
AdcDriver* AdcDriver::s_instance_ = nullptr;

AdcDriver::~AdcDriver() {
  Stop();
}

bool AdcDriver::Init(const Config& config) {
  if (!config.IsValid()) {
    ULOG_ERROR("AdcDriver: Invalid configuration");
    return false;
  }

  // Stop if already initialized
  if (state_ != State::STOPPED) {
    Stop();
  }

  config_ = config;

  // Initialize conversion group
  conv_group_.circular = config.circular;
  conv_group_.num_channels = config.num_channels;
  conv_group_.end_cb = ConversionEndCallback;
  conv_group_.error_cb = ConversionErrorCallback;
  conv_group_.cfgr = ADC_CFGR_CONT_ENABLED | ADC_CFGR_DMNGT_CIRCULAR;
  conv_group_.cfgr2 = 0;
  conv_group_.ccr = 0;
  conv_group_.pcsel = 0;
  conv_group_.ltr1 = 0;
  conv_group_.htr1 = 0;
  conv_group_.ltr2 = 0;
  conv_group_.htr2 = 0;
  conv_group_.ltr3 = 0;
  conv_group_.htr3 = 0;
  conv_group_.awd2cr = 0;
  conv_group_.awd3cr = 0;

  // Clear SMPR registers
  conv_group_.smpr[0] = 0;
  conv_group_.smpr[1] = 0;

  // Set sampling time for all channels
  // For simplicity, we use the same sampling time for all channels
  // In a more advanced implementation, we could support per-channel sampling times
  for (uint8_t i = 0; i < config.num_channels; ++i) {
    if (config.channels[i] <= 9) {
      conv_group_.smpr[0] |= config.sampling_time << (config.channels[i] * 3);
    } else if (config.channels[i] <= 19) {
      conv_group_.smpr[1] |= config.sampling_time << ((config.channels[i] - 10) * 3);
    } else {
      ULOG_ERROR("AdcDriver: Channel %lu out of range", config.channels[i]);
      return false;
    }
  }

  // Clear SQR registers
  conv_group_.sqr[0] = 0;
  conv_group_.sqr[1] = 0;
  conv_group_.sqr[2] = 0;
  conv_group_.sqr[3] = 0;

  // Set sequence order (same as input order)
  for (uint8_t i = 0; i < config.num_channels; ++i) {
    uint8_t sq_index = i + 1;  // SQR positions start at 1
    if (sq_index <= 6) {
      conv_group_.sqr[0] |= config.channels[i] << ((sq_index - 1) * 5);
    } else if (sq_index <= 12) {
      conv_group_.sqr[1] |= config.channels[i] << ((sq_index - 7) * 5);
    } else if (sq_index <= 18) {
      conv_group_.sqr[2] |= config.channels[i] << ((sq_index - 13) * 5);
    } else if (sq_index <= 20) {
      conv_group_.sqr[3] |= config.channels[i] << ((sq_index - 19) * 5);
    } else {
      ULOG_ERROR("AdcDriver: Too many channels (%u), max is 20", config.num_channels);
      return false;
    }
  }

  // Set number of conversions in sequence
  conv_group_.sqr[0] |= ADC_SQR1_NUM_CH(config.num_channels);

  // Initialize binary semaphore for conversion completion
  chSemObjectInit(&conversion_sem_, 0);

  // Set static instance pointer (single instance assumption)
  s_instance_ = this;

  state_ = State::READY;
  ULOG_INFO("AdcDriver: Initialized with %u channels, buffer depth %zu", config.num_channels, config.buffer_depth);
  return true;
}

bool AdcDriver::Start() {
  if (state_ != State::READY) {
    ULOG_ERROR("AdcDriver: Cannot start, not in READY state");
    return false;
  }

  msg_t result = StartADC();
  if (result != HAL_RET_SUCCESS) {
    state_ = State::ERROR;
    ULOG_ERROR("AdcDriver: Failed to start ADC (result %d)", result);
    return false;
  }

  ULOG_DEBUG("AdcDriver: Started");
  return true;
}

void AdcDriver::Stop() {
  if (state_ == State::STOPPED || state_ == State::ERROR) {
    return;
  }

  StopConversion();
  StopADC();
  // Clear static instance if it's this instance
  if (s_instance_ == this) {
    s_instance_ = nullptr;
  }

  state_ = State::STOPPED;
  ULOG_DEBUG("AdcDriver: Stopped");
}

bool AdcDriver::StartConversion() {
  if (state_ != State::READY) {
    ULOG_ERROR("AdcDriver: Cannot start conversion, not in READY state");
    return false;
  }

  // For single conversion, temporarily set circular to false
  bool was_circular = conv_group_.circular;
  conv_group_.circular = false;

  // Reset semaphore (ensure it's empty)
  chSemReset(&conversion_sem_, 0);

  adcStartConversion(config_.adc, &conv_group_, config_.buffer, config_.buffer_depth * config_.num_channels);

  conv_group_.circular = was_circular;
  state_ = State::CONVERTING;
  return true;
}

bool AdcDriver::StartContinuous() {
  if (state_ != State::READY) {
    ULOG_ERROR("AdcDriver: Cannot start continuous conversion, not in READY state");
    return false;
  }

  // Ensure circular mode is enabled
  conv_group_.circular = true;

  adcStartConversion(config_.adc, &conv_group_, config_.buffer, config_.buffer_depth * config_.num_channels);

  state_ = State::CONVERTING;
  return true;
}

void AdcDriver::StopConversion() {
  if (state_ != State::CONVERTING) {
    return;
  }

  adcStopConversion(config_.adc);
  state_ = State::READY;

  // Signal semaphore to wake up any waiting thread
  chSemSignal(&conversion_sem_);
}

bool AdcDriver::WaitConversion(systime_t timeout) {
  if (state_ != State::CONVERTING) {
    return true;  // Not converting, so "completed"
  }

  // Wait on semaphore with timeout
  msg_t msg = chSemWaitTimeout(&conversion_sem_, timeout);
  if (msg == MSG_TIMEOUT) {
    ULOG_WARNING("AdcDriver: Conversion timeout");
    return false;
  }

  // If semaphore was signaled, conversion completed
  return state_ == State::READY;
}

float AdcDriver::RawToVoltage(adcsample_t sample) const {
  // Assuming 12-bit ADC (0-4095)
  return (sample * config_.vref) / 4095.0f;
}

float AdcDriver::GetChannelVoltage(uint8_t channel) const {
  if (channel >= config_.num_channels || config_.buffer == nullptr) {
    return 0.0f;
  }

  // Calculate average of all samples for this channel
  uint32_t sum = 0;
  for (size_t i = 0; i < config_.buffer_depth; ++i) {
    sum += config_.buffer[i * config_.num_channels + channel];
  }

  float avg = static_cast<float>(sum) / config_.buffer_depth;
  return RawToVoltage(static_cast<adcsample_t>(avg));
}

adcsample_t AdcDriver::GetChannelSample(uint8_t channel) const {
  if (channel >= config_.num_channels || config_.buffer == nullptr || config_.buffer_depth == 0) {
    return 0;
  }

  // Return the latest sample (last in buffer)
  return config_.buffer[(config_.buffer_depth - 1) * config_.num_channels + channel];
}

msg_t AdcDriver::StartADC() {
  if (config_.adc == nullptr) {
    return HAL_RET_NO_RESOURCE;
  }

  // Check if ADC is already active
  if (config_.adc->state == ADC_READY || config_.adc->state == ADC_ACTIVE) {
    ULOG_DEBUG("AdcDriver: ADC already started, skipping");
    return HAL_RET_SUCCESS;
  }

  // Start ADC driver with default configuration
  msg_t result = adcStart(config_.adc, nullptr);
  if (result != HAL_RET_SUCCESS) {
    // Not a real error if used via DmaResourceManager
    ULOG_DEBUG("AdcDriver: Failed to start ADC peripheral (result %d)", result);
    return result;
  }

  return HAL_RET_SUCCESS;
}

void AdcDriver::StopADC() {
  if (config_.adc == nullptr) {
    return;
  }

  // Stop ADC driver
  adcStop(config_.adc);
}

void AdcDriver::ConversionEndCallback(ADCDriver* adcp) {
  // Use static instance pointer (single instance assumption)
  if (s_instance_ == nullptr || s_instance_->config_.adc != adcp) {
    // Instance mismatch or not set; ignore
    return;
  }

  // Kernel is already locked by ADC ISR, but we need to satisfy debug check
  // Use chSysLockFromISR() and chSysUnlockFromISR() to explicitly lock/unlock
  chSysLockFromISR();
  chSemSignalI(&s_instance_->conversion_sem_);
  chSysUnlockFromISR();

  // Update state
  s_instance_->state_ = State::READY;
}

void AdcDriver::ConversionErrorCallback(ADCDriver* adcp, [[maybe_unused]] adcerror_t err) {
  // Use static instance pointer (single instance assumption)
  if (s_instance_ == nullptr || s_instance_->config_.adc != adcp) {
    // Instance mismatch or not set; ignore
    return;
  }

  // Signal semaphore from ISR to wake up waiting thread
  chSemSignalI(&s_instance_->conversion_sem_);

  s_instance_->state_ = State::ERROR;
}

}  // namespace xbot::driver::adc
