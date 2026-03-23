/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sound_driver.cpp
 * @brief High-Level Sound Driver for STM32H723 with MAX98357
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-03-19
 *
 * @note  Audio playback is driven by BDMA via the ChibiOS I2S HAL (I2SD6).
 *        The DMA buffer resides in SRAM4 (D3 domain) as required by BDMA.
 *        Blocking calls (beep, play_buffer) suspend the calling thread and
 *        resume it from the BDMA end-of-buffer ISR callback.
 */

#include "sound_driver.hpp"

#include <ulog.h>

#include "hal.h"
#include "sound_samples.hpp"

namespace xbot::driver::sound {

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/** Total int16_t elements in the double-buffer (two halves for DMA ping-pong). */
static constexpr size_t SOUND_BUFFER_SIZE = 1024U;
/** Elements per DMA half (512 = 256 stereo frames = 16 ms at 16 kHz). */
static constexpr size_t SOUND_HALF_SIZE = SOUND_BUFFER_SIZE / 2U;

/*===========================================================================*/
/* DMA buffer — MUST reside in SRAM4 (D3 domain) for BDMA.                  */
/*===========================================================================*/

static int16_t audio_buffer[SOUND_BUFFER_SIZE] __attribute__((section(".sram4")));

/*===========================================================================*/
/* I2S configuration and driver state.                                       */
/*===========================================================================*/

static void i2s_end_cb(I2SDriver *i2sp);

static I2SConfig i2s_cfg = {
    /* tx_buffer / rx_buffer / size / end_cb come first (portable fields). */
    .tx_buffer = audio_buffer,
    .rx_buffer = nullptr,
    .size = SOUND_BUFFER_SIZE,
    .end_cb = i2s_end_cb,
    /* LLD-specific fields (from i2s_lld_config_fields). */
    .sample_rate = 16000U,
    .i2scfgr = 0U, /* Philips, DATLEN=16-bit, CHLEN=16-bit */
};

/*===========================================================================*/
/* Playback state (shared between thread and ISR callback).                  */
/*===========================================================================*/

enum class PlayMode : uint8_t { NONE, TONE, BUFFER };

static struct {
  PlayMode mode;
  /* TONE fields */
  uint32_t phase;
  uint32_t phase_increment;
  uint32_t samples_remaining;
  /* BUFFER fields */
  const audio_buffer_t *buffer;
  uint32_t buffer_pos;
  /* Common */
  uint8_t volume;
} playback;

static sound_config_t current_config = {.sample_rate = 16000U, .volume = 80U, .stereo = true};
static bool is_initialized = false;
static bool signal_pending = false;
static binary_semaphore_t done_sem;

/*===========================================================================*/
/* Internal helpers.                                                         */
/*===========================================================================*/

/**
 * @brief   Fill @p count int16_t slots (stereo L+R pairs) from the current
 *          playback source.  Writes silence once the source is exhausted and
 *          marks playback.mode = NONE on the first exhausted half.
 */
static void fill_half(int16_t *buf, size_t count) {
  for (size_t i = 0U; i < count; i += 2U) {
    int16_t sample = 0;

    if (playback.mode == PlayMode::TONE) {
      if (playback.samples_remaining > 0U) {
        sample = samples::generate_sine_sample_fp(playback.phase);
        sample = samples::apply_volume(sample, playback.volume);
        playback.phase += playback.phase_increment;
        --playback.samples_remaining;
      } else {
        playback.mode = PlayMode::NONE;
      }

    } else if (playback.mode == PlayMode::BUFFER && playback.buffer != nullptr) {
      if (playback.buffer_pos < playback.buffer->length) {
        sample = playback.buffer->data[playback.buffer_pos];
        sample = samples::apply_volume(sample, playback.volume);
        ++playback.buffer_pos;
        if (playback.buffer->loop && playback.buffer_pos >= playback.buffer->length) {
          playback.buffer_pos = 0U;
        }
      } else {
        playback.mode = PlayMode::NONE;
      }
    }

    buf[i] = sample;     /* Left  channel */
    buf[i + 1] = sample; /* Right channel */
  }
}

/**
 * @brief   I2S BDMA end-of-half / end-of-buffer callback (ISR context).
 *
 * @details Called by the portable ChibiOS I2S layer:
 *            - HTIF (half done, state=I2S_ACTIVE)    → refill second half
 *            - TCIF (full done, state=I2S_COMPLETE)  → refill first half
 */
static void i2s_end_cb(I2SDriver *i2sp) {
  /* Determine which half the DMA just finished playing and needs refilling. */
  int16_t *half = i2sIsBufferComplete(i2sp) ? audio_buffer                      /* TCIF: refill first half  */
                                            : (audio_buffer + SOUND_HALF_SIZE); /* HTIF: refill second half */

  fill_half(half, SOUND_HALF_SIZE);

  /* Signal the blocked thread once when the source is exhausted. */
  if (playback.mode == PlayMode::NONE && !signal_pending) {
    signal_pending = true;
    chSysLockFromISR();
    chBSemSignalI(&done_sem);
    chSysUnlockFromISR();
  }
}

/*===========================================================================*/
/* Public API.                                                               */
/*===========================================================================*/

bool init(const sound_config_t *config) {
  if (config != nullptr) {
    current_config = *config;
  }

  i2s_cfg.sample_rate = current_config.sample_rate;
  playback.mode = PlayMode::NONE;

  chBSemObjectInit(&done_sem, true); /* Initially taken — thread will wait on it. */

  i2sStart(&I2SD6, &i2s_cfg);
  is_initialized = true;

  ULOG_INFO("Sound: initialized (sample_rate=%u, volume=%u)", current_config.sample_rate, current_config.volume);
  return true;
}

void start(void) {
  if (!is_initialized) {
    init(nullptr);
  }
}

void stop(void) {
  if (is_initialized) {
    i2sStop(&I2SD6);
    is_initialized = false;
    playback.mode = PlayMode::NONE;
  }
}

void set_volume(uint8_t volume) {
  if (volume > 100U) volume = 100U;
  current_config.volume = volume;
}

bool is_playing(void) {
  return playback.mode != PlayMode::NONE;
}

void beep(uint32_t frequency, uint32_t duration_ms, uint8_t volume) {
  if (frequency == 0U || duration_ms == 0U) return;
  if (!is_initialized) init(nullptr);

  /* Set up tone playback state. */
  playback.mode = PlayMode::TONE;
  playback.phase = 0U;
  playback.phase_increment = samples::calculate_phase_increment(frequency, current_config.sample_rate);
  playback.samples_remaining = (current_config.sample_rate * duration_ms) / 1000U;
  playback.volume = (volume <= 100U) ? volume : current_config.volume;
  signal_pending = false;

  /* Pre-fill both halves so DMA has valid data from the first moment. */
  fill_half(audio_buffer, SOUND_HALF_SIZE);
  fill_half(audio_buffer + SOUND_HALF_SIZE, SOUND_HALF_SIZE);

  /* Start DMA and block the calling thread until playback is complete. */
  i2sStartExchange(&I2SD6);
  chBSemWait(&done_sem);
  i2sStopExchange(&I2SD6);
}

void play_buffer(const audio_buffer_t *buffer) {
  if (buffer == nullptr || buffer->data == nullptr || buffer->length == 0U) return;
  if (!is_initialized) init(nullptr);

  /* Set up buffer playback state. */
  playback.mode = PlayMode::BUFFER;
  playback.buffer = buffer;
  playback.buffer_pos = 0U;
  playback.volume = current_config.volume;
  signal_pending = false;

  /* Pre-fill both halves before starting DMA. */
  fill_half(audio_buffer, SOUND_HALF_SIZE);
  fill_half(audio_buffer + SOUND_HALF_SIZE, SOUND_HALF_SIZE);

  i2sStartExchange(&I2SD6);

  if (!buffer->loop) {
    /* Blocking: wait until all samples are played. */
    chBSemWait(&done_sem);
    i2sStopExchange(&I2SD6);
  }
  /* Looping buffers play indefinitely; caller must invoke stop() to halt. */
}

void poll(void) {
  /* No-op: playback is now driven by BDMA ISR callbacks. */
}

void test_tone(void) {
  beep(440, 1000, 90);
}
void error_beep(void) {
  beep(1000, 200, 90);
}
void success_beep(void) {
  beep(300, 300, 80);
}

void warning_beep(void) {
  beep(600, 150, 85);
  chThdSleepMilliseconds(50);
  beep(600, 150, 85);
}

void play_test_tone(void) {
  test_tone();
}

void demo(void) {
  init(nullptr);
  success_beep();
  chThdSleepMilliseconds(100);
  warning_beep();
  chThdSleepMilliseconds(100);
  error_beep();
  chThdSleepMilliseconds(100);
  test_tone();
}

}  // namespace xbot::driver::sound
