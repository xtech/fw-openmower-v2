/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/**
 * @file adc1.cpp
 * @brief ADC1 driver with hardware-independent configuration
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-01-25
 */

#include "adc1.hpp"

#include <ch.h>
#include <ulog.h>

#include <cstring>

#include "adc3.hpp"

using namespace xbot::driver;

namespace xbot::driver::adc1 {

// Private
namespace {
// Lookup: Sensor-ID -> Conversion group (or -1 if not present)
Adc1ConversionGroup* conv_id_to_cg_[static_cast<uint8_t>(Adc1ConversionId::_NUM_CHANNEL_IDS)] = {};
}  // namespace

void Init() {
  // ADC1 got started automatically by halInit(),
  // but we need ADC3 for a precise Vrefint
  adc3::Init();
}

bool Start() {
  if (ADCD1.state != ADC_STOP && ADCD1.state != ADC_READY) {
    ULOG_ERROR("ADC1: Not initialized");
    return false;
  }

  // Start ADC driver with linear calibration
  static const ADCConfig adc1_config = {.difsel = 0x0, .calibration = ADC_CR_ADCALLIN};
  msg_t result = adcStart(&ADCD1, &adc1_config);
  if (result != HAL_RET_SUCCESS) {
    ULOG_ERROR("ADC1: Failed to start (result %d)", result);
    return false;
  }

  // We also need to start ADC3
  adc3::Start();

  return true;
}

bool Convert(Adc1ConversionGroup* cg) {
  ADCConversionGroup adc_cg{};
  auto res_info = GetResolutionInfo(cg->resolution);

  // Initialize conversion group
  adc_cg.circular = false;
  adc_cg.num_channels = static_cast<adc_channels_num_t>(cg->sensors.size());
  adc_cg.end_cb = NULL;
  adc_cg.error_cb = NULL;
  adc_cg.cfgr = res_info.adc_mask;
  adc_cg.cfgr2 = (cg->ovs_ratio > 0 ? (cg->ovs_ratio - 1) << ADC_CFGR2_OVSR_Pos : 0) |  // Oversampling ratio
                 (cg->ovs_rshift << ADC_CFGR2_OVSS_Pos) |                               // Oversampling right-shift
                 (cg->ovs_ratio > 1 ? ADC_CFGR2_ROVSE : 0);                             // Oversampling enable

  // PCSEL, SMPR, SQR setup
  for (size_t i = 0; i < cg->sensors.size(); ++i) {
    adc_channels_num_t channel = cg->sensors[i].channel;

    // PCSEL
    adc_cg.pcsel |= (1U << channel);

    // SMPR
    if (channel <= ADC_CHANNEL_IN9) {
      adc_cg.smpr[0] |= cg->sensors[i].sample_rate << (channel * 3);
    } else if (channel <= ADC_CHANNEL_IN19) {
      adc_cg.smpr[1] |= cg->sensors[i].sample_rate << ((channel - 10) * 3);
    }

    // SQR
    if (i < 4) {
      adc_cg.sqr[0] |= ADC_SQR1_SQ1_N(channel) << (i * 6);
    } else if (i < 8) {
      adc_cg.sqr[1] |= ADC_SQR2_SQ5_N(channel) << ((i - 4) * 6);
    } else if (i < 13) {
      adc_cg.sqr[2] |= ADC_SQR3_SQ10_N(channel) << ((i - 8) * 6);
    } else if (i < 16) {
      adc_cg.sqr[3] |= ADC_SQR4_SQ15_N(channel) << ((i - 12) * 6);
    }
  }

  // Synchronous conversion (blocking)
  adcAcquireBus(&ADCD1);
  msg_t result = adcConvert(&ADCD1, &adc_cg, cg->sample_buffer, 1);
  if (result != MSG_OK) {
    ULOG_ERROR("adc1::Convert: Failed (result %d)", result);
  }
  adcReleaseBus(&ADCD1);

  return (result == MSG_OK);
}

float GetValueOrNaN(Adc1ConversionId conv_id, uint16_t max_age_ms) {
  float ret = std::numeric_limits<float>::quiet_NaN();

  Adc1ConversionGroup* cg = GetConversionGroup(conv_id);
  if (!cg || !cg->IsValid()) return ret;  // Conversion group doesn't exists or is invalid

  // Check cache first
  if (cg->IsCacheValid(max_age_ms)) {
    return cg->cached_value;
  }

  // Acquire ADC3 for exclusive access
  // Attention: Because Convert() will Acquire(&ADCD1),
  // this might deadlock if some other code also use ADC3 and ADC1 but lock in another order!
  adc3::Acquire();

  // Immediately before conversion of the requested ADC1 channel, start an async Vref conversion in parallel on ADC3
  adc3::StartConvert(ADC_CHANNEL_IN18, adc3::SampleRate::CYCLES_24P5, xbot::driver::adc3::Resolution::BITS_12,
                     adc3::OversampleRatio::X16, adc3::OversampleShift::SHIFT_4);

  // Perform synchronous conversion of our channel(s)
  bool conv_result = Convert(cg);

  // Wait for Vref conversion of ADC3
  uint16_t vref_raw = adc3::WaitForConversion();

  // Release ADC3
  adc3::Release();

  if (!conv_result) {
    return ret;
  }

  float vref = adc3::GetVrefVoltage(vref_raw);

  // Call the conversion group's delegate to process the raw data
  float value = cg->convert(cg, vref);

  // Update cache
  cg->UpdateCache(value);

  return value;
}

bool RegisterConversionGroup(const Adc1ConversionGroup& cg) {
  if (!cg.IsValid()) {
    ULOG_ERROR("RegisterConversionGroup: Invalid conversion group %u", cg.id);
    return false;
  }

  uint8_t id_num = static_cast<uint8_t>(cg.id);
  if (id_num >= static_cast<uint8_t>(Adc1ConversionId::_NUM_CHANNEL_IDS)) {
    ULOG_ERROR("RegisterConversionGroup: ID %u out of range", id_num);
    return false;
  }

  if (conv_id_to_cg_[id_num] != nullptr) {
    ULOG_WARNING("RegisterConversionGroup: ID %u already registered. Overwriting ...", id_num);
  }

  conv_id_to_cg_[id_num] = const_cast<Adc1ConversionGroup*>(&cg);
  return true;
}

Adc1ConversionGroup* GetConversionGroup(Adc1ConversionId id) {
  uint8_t id_num = static_cast<uint8_t>(id);
  return conv_id_to_cg_[id_num];
}

void DumpBenchmarkMeasurement(Adc1ConversionId conv_id, const char* sensor_name, size_t num_samples, bool verbose) {
  float sum = 0;
  float min_val = std::numeric_limits<float>::max();
  float max_val = std::numeric_limits<float>::lowest();
  size_t min_cycles = SIZE_MAX;
  size_t max_cycles = 0;
  size_t sum_cycles = 0;

  // Vars for sequential diff
  float prev_value = std::numeric_limits<float>::quiet_NaN();
  float min_seq_diff = std::numeric_limits<float>::max();
  float max_seq_diff = std::numeric_limits<float>::lowest();
  float sum_seq_diff = 0;
  size_t num_seq_diffs = 0;

  for (size_t i = 0; i < num_samples; i++) {
    size_t start = chSysGetRealtimeCounterX();
    float value = GetValueOrNaN(conv_id, 0);
    size_t end = chSysGetRealtimeCounterX();
    size_t cycles = end - start;

    sum += value;
    sum_cycles += cycles;

    min_val = std::min(min_val, value);
    max_val = std::max(max_val, value);
    min_cycles = std::min(min_cycles, cycles);
    max_cycles = std::max(max_cycles, cycles);

    // Calc sequential diff
    if (!std::isnan(prev_value)) {
      float seq_diff = std::abs(value - prev_value);
      min_seq_diff = std::min(min_seq_diff, seq_diff);
      max_seq_diff = std::max(max_seq_diff, seq_diff);
      sum_seq_diff += seq_diff;
      num_seq_diffs++;
    }

    if (verbose) {
      ULOG_INFO("adc1:%s #%2u: %.4fV, Δ%.4fV, %u cycles", sensor_name, i, value, value - prev_value, cycles);
    }

    prev_value = value;
  }

  // Calculate averages
  float avg_value = sum / num_samples;
  size_t avg_cycles = sum_cycles / num_samples;
  float avg_seq_diff = (num_seq_diffs > 0) ? (sum_seq_diff / num_seq_diffs) : 0;

  // Dump summary
  ULOG_INFO("adc1:%s %u reads: avg %.4f (%.4f - %.4f), avg Δ%.5f (%.5f - %.5f), avg %ucyc (%u - %u)", sensor_name,
            num_samples, avg_value, min_val, max_val, avg_seq_diff, min_seq_diff, max_seq_diff, avg_cycles, min_cycles,
            max_cycles);
}

}  // namespace xbot::driver::adc1
