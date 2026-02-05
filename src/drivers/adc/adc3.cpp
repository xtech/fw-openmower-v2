/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file adc3.cpp
 * @brief Minimal ADC3 driver for VREFINT measurement
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-01-30
 */

#include "adc3.hpp"

#include <hal.h>

namespace xbot::driver::adc3 {

namespace {
mutex_t adc3_mutex;

// Initialize in Init() or first use
void InitMutex() {
  static bool initialized = false;
  if (!initialized) {
    chMtxObjectInit(&adc3_mutex);
    initialized = true;
  }
}
}  // namespace

// ADC configuration
constexpr uint8_t ADC_BITS = 12;
constexpr uint16_t ADC_MAX_VALUE = (1 << ADC_BITS) - 1;

// TS_CAL1 for VREFINT
inline const uint16_t VREFINT_CAL_VALUE = *reinterpret_cast<const volatile uint16_t*>(0x1FF1E860UL);
inline const float VREFINT_CAL_VALUE_F = static_cast<float>(VREFINT_CAL_VALUE);
static constexpr float ADC_REFERENCE_VOLTAGE = 3.3f;  // ADC reference voltage (VDDA during factory calibration)

void Init() {
  InitMutex();

  rccEnableADC3(true);
  rccResetADC3();

  // Clock mode: AHB clock / 4
  // With 25MHz HSE, PLL1 configured for 550MHz system clock
  // AHB clock = 550MHz / 2 (D1HPRE) = 275MHz
  // ADC clock = 275MHz / 4 = 68.75MHz (ADC3 range is 1.5-75MHz)
  ADC3_COMMON->CCR = ADC_CCR_CKMODE_AHB_DIV4;

  rccDisableADC3();
  return;
}

void Start() {
  rccEnableADC3(true);

  // Only initialize if not already enabled
  if (!(ADC3->CR & ADC_CR_ADEN)) {
    // Enable voltage regulator
    ADC3->CR = ADC_CR_ADVREGEN;  // This will also end DEEPPWD
    chThdSleepMilliseconds(1);   // Small delay for VREFINT to stabilize

    // Perform single-ended calibration
    osalDbgAssert(ADC3->CR == ADC_CR_ADVREGEN, "invalid register state");
    ADC3->CR &= ~ADC_CR_ADCALDIF;
    ADC3->CR |= ADC_CR_ADCAL;
    while (ADC3->CR & ADC_CR_ADCAL)
      ;  // Wait for calibration to complete

    // Enable ADC
    ADC3->ISR = ADC_ISR_ADRDY;
    ADC3->CR |= ADC_CR_ADEN;
    while ((ADC3->ISR & ADC_ISR_ADRDY) == 0)
      ;  // Wait for ADC to be ready
  }

  // Enable Vrefint reading
  ADC3_COMMON->CCR |= ADC_CCR_VREFEN;
  chThdSleepMilliseconds(1);  // Small delay for VREFINT to stabilize

  return;
}

void StartConvert(adc_channels_num_t chan, SampleRate smpr, Resolution resolution, OversampleRatio ovsr,
                  OversampleShift ovss) {
  ADC3->CFGR = static_cast<uint8_t>(resolution) << ADC3_CFGR_RES_Pos;  // Resolution
  ADC3->CFGR2 = 0;
  if (ovsr != OversampleRatio::NONE) {
    ADC3->CFGR2 |= static_cast<uint8_t>(ovsr) << ADC3_CFGR2_OVSR_Pos;  // Oversample ratio
    ADC3->CFGR2 |= static_cast<uint8_t>(ovss) << ADC_CFGR2_OVSS_Pos;   // Oversample shift
    ADC3->CFGR2 |= ADC_CFGR2_ROVSE << ADC_CFGR2_ROVSE_Pos;             // Enable oversampling
  }
  ADC3->SQR1 = ADC_SQR1_SQ1_N(chan) | ADC_SQR1_NUM_CH(1);
  ADC3->SQR2 = 0;
  ADC3->SQR3 = 0;
  ADC3->SQR4 = 0;
  ADC3->SMPR1 = 0;
  ADC3->SMPR2 = 0;

  // SMPR
  if (chan <= ADC_CHANNEL_IN9) {
    ADC3->SMPR1 |= static_cast<uint8_t>(smpr) << (chan * 3);
  } else if (chan <= ADC_CHANNEL_IN18) {
    ADC3->SMPR2 |= static_cast<uint8_t>(smpr) << ((chan - 10) * 3);
  }

  // Start conversion
  ADC3->CR |= ADC_CR_ADSTART;
}

uint16_t WaitForConversion() {
  // ATTENTION: Debugging this loop will not work because your XPERIPHERALS will read DR and thus clear EOC
  while (!(ADC3->ISR & ADC_ISR_EOC)) {
    // FIXME: Remove when finished debugging chThdSleepMicroseconds(5);  // We're not in hurry
  }
  return ADC3->DR;
}

float GetVrefVoltage(uint16_t raw_value) {
  // Calculate actual VDDA voltage using formula from STM32 reference manual:
  // VDDA_actual = ADC_REFERENCE_VOLTAGE × VREFINT_CAL / VREFINT_DATA
  // Where:
  // - ADC_REFERENCE_VOLTAGE is VDDA voltage during factory calibration (3.3V)
  // - VREFINT_CAL is factory calibration value
  // - VREFINT_DATA is actual ADC reading (raw_value)
  float vdda = (ADC_REFERENCE_VOLTAGE * VREFINT_CAL_VALUE_F) / static_cast<float>(raw_value);
  return vdda;
}

void Acquire() {
  chMtxLock(&adc3_mutex);
}

void Release() {
  chMtxUnlock(&adc3_mutex);
}

}  // namespace xbot::driver::adc3
