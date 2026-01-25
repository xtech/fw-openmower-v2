/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/**
 * @file adc_driver.cpp
 * @brief ADC driver with hardware-independent configuration
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-01-25
 */

#include "adc_driver.hpp"

#include <ch.h>
#include <ulog.h>

#include <cstring>

namespace xbot::driver::adc {

bool AdcDriver::Init(const AdcConfig &config) {
  if (!config.IsValid()) {
    ULOG_ERROR("AdcDriver: Invalid configuration");
    return false;
  }

  config_ = config;

  // Initialize conversion group
  conv_group_.circular = false;
  conv_group_.num_channels = static_cast<adc_channels_num_t>(config.channels.size());
  conv_group_.end_cb = NULL;
  conv_group_.error_cb = NULL;
  conv_group_.cfgr = ADC_CFGR_RES_16BITS;

  // PCSEL, SMPR, SQR setup
  for (size_t i = 0; i < config.channels.size(); ++i) {
    adc_channels_num_t channel = config.channels[i].channel;

    // PCSEL
    conv_group_.pcsel |= (1U << channel);

    // SMPR
    if (channel <= ADC_CHANNEL_IN9) {
      conv_group_.smpr[0] |= config.channels[i].sample_rate << (channel * 3);
    } else if (channel <= ADC_CHANNEL_IN19) {
      conv_group_.smpr[1] |= config.channels[i].sample_rate << ((channel - 10) * 3);
    }

    // SQR
    if (i < 4) {
      conv_group_.sqr[0] |= ADC_SQR1_SQ1_N(channel) << (i * 6);
    } else if (i < 8) {
      conv_group_.sqr[1] |= ADC_SQR2_SQ5_N(channel) << ((i - 4) * 6);
    } else if (i < 13) {
      conv_group_.sqr[2] |= ADC_SQR3_SQ10_N(channel) << ((i - 8) * 6);
    } else if (i < 16) {
      conv_group_.sqr[3] |= ADC_SQR4_SQ15_N(channel) << ((i - 12) * 6);
    }

    // Map ChannelId to channel index
    uint8_t id_num = static_cast<uint8_t>(config.channels[i].id);
    channel_id_to_index_[id_num] = static_cast<int8_t>(i);
  }

  // Set number of conversions in sequence
  conv_group_.sqr[0] |= ADC_SQR1_NUM_CH(config.channels.size());

  initialized_ = true;

  ULOG_INFO("AdcDriver: Initialized with %u channels", config.channels.size());

  return true;
}

bool AdcDriver::Start() {
  if (!initialized_) {
    ULOG_ERROR("AdcDriver: Not initialized");
    return false;
  }

  if (config_.drv->state != ADC_STOP) {
    ULOG_WARNING("AdcDriver: Invalid, !ADC_STOP state");
    return false;
  }

  // Start ADC driver with ChibiOS default config (diffsel=0)
  msg_t result = adcStart(config_.drv, nullptr);
  if (result != HAL_RET_SUCCESS) {
    ULOG_ERROR("AdcDriver: Failed to start ADC (result %d)", result);
    return false;
  }

  ULOG_DEBUG("AdcDriver: Started");
  return true;
}

void AdcDriver::Stop() {
  if (!initialized_) {
    return;
  }
  adcStop(config_.drv);
  ULOG_DEBUG("AdcDriver: Stopped");
}

msg_t AdcDriver::Convert() {
  if (!initialized_) {
    ULOG_ERROR("AdcDriver: Not initialized");
    return MSG_RESET;
  }

  adcAcquireBus(config_.drv);
  // Synchronous conversion (blocking)
  msg_t result = adcConvert(config_.drv, &conv_group_, config_.sample_buffer, 1);
  if (result == MSG_OK) {
    last_conversion_ = chVTGetSystemTimeX();
  } else {
    ULOG_ERROR("AdcDriver: adcConvert failed (result %d)", result);
  }
  adcReleaseBus(config_.drv);

  return result;
}

void AdcDriver::ConvertIfOutdated(uint16_t max_age_ms) {
  if (!initialized_) return;
  // Age not reached
  if ((last_conversion_ + TIME_MS2I(max_age_ms)) > chVTGetSystemTimeX()) return;

  Convert();
}

adcsample_t AdcDriver::GetChannelRawValue(ChannelId channel_id, uint16_t max_age_ms) {
  if (!initialized_) return 0;

  ConvertIfOutdated(max_age_ms);

  auto channel_index = channel_id_to_index_[static_cast<uint8_t>(channel_id)];
  if (channel_index < 0) return 0;

  ULOG_INFO("AdcDriver: GetChannelRawValue raw=%u (ID=%d index=%d  age=%ums)", config_.sample_buffer[channel_index],
            channel_id, channel_index, TIME_I2MS(chVTGetSystemTimeX() - last_conversion_));

  return config_.sample_buffer[channel_index];
}

float AdcDriver::GetChannelValue(ChannelId channel_id, uint16_t max_age_ms) {
  if (!initialized_) {
    return std::numeric_limits<float>::quiet_NaN();
  }

  const int8_t channel_index = channel_id_to_index_[static_cast<uint8_t>(channel_id)];
  if (channel_index < 0) return std::numeric_limits<float>::quiet_NaN();

  const AdcChannel &channel = config_.channels[channel_index];
  const adcsample_t raw = GetChannelRawValue(channel_id, max_age_ms);

  float val = std::numeric_limits<float>::quiet_NaN();
  if (channel.convert.is_valid()) {
    val = channel.convert(raw);
  } else {
    val = AdcChannel::RawToVoltage(raw);
  }

  ULOG_INFO("AdcDriver: GetChannelValue %.3f (ID=%d index=%d  age=%ums)", val, channel_id, channel_index,
            TIME_I2MS(chVTGetSystemTimeX() - last_conversion_));

  return val;
}

}  // namespace xbot::driver::adc
