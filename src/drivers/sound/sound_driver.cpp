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
 */

#include "sound_driver.hpp"

#include <cstddef>

#include "i2s6_lld.hpp"
#include "sound_samples.hpp"

/* Debug logging */
#include <ulog.h>

namespace xbot::driver::sound {

/* Default configuration - Updated for MAX98357 I2S mode */
/* 16kHz sample rate according to architecture documentation */
static const sound_config_t default_config = {
    .sample_rate = 16000, /* 16 kHz - according to architecture documentation */
    .volume = 80,         /* 80% volume */
    .stereo = true,
};

/* Current configuration */
static sound_config_t current_config = default_config;

/* Current playback state */
static const audio_buffer_t* current_buffer = NULL;
static uint32_t buffer_position = 0;
static bool playing_state = false;
static bool is_initialized = false;

/* I2S6 configuration - Updated for MAX98357 I2S mode (SD_MODE=0, GAIN_SLOT=0) */
static i2s6_config_t i2s_config = {
    .sample_rate = 16000, /* 16 kHz - according to architecture documentation */
    .data_bits = 16,
    .channel_bits = 16,
    .master_mode = true,
    .i2s_standard = true, /* I2S Philips standard */
    .i2s_mode = true,     /* I2S mode (not PCM) */
};

bool init(const sound_config_t* config) {
  ULOG_INFO("Sound: Initializing sound driver");

  if (config != NULL) {
    current_config = *config;
    ULOG_INFO("Sound: Using custom config: sample_rate=%u, volume=%u, stereo=%s", current_config.sample_rate,
              current_config.volume, current_config.stereo ? "true" : "false");
  } else {
    current_config = default_config;
    ULOG_INFO("Sound: Using default config: sample_rate=%u, volume=%u, stereo=%s", current_config.sample_rate,
              current_config.volume, current_config.stereo ? "true" : "false");
  }

  /* Update I2S configuration */
  i2s_config.sample_rate = current_config.sample_rate;
  ULOG_INFO("Sound: Configuring I2S6 with sample_rate=%u", i2s_config.sample_rate);

  /* Initialize I2S6 */
  if (!i2s6::init(&i2s_config)) {
    ULOG_ERROR("Sound: Failed to initialize I2S6");
    return false;
  }

  is_initialized = true;
  ULOG_INFO("Sound: Initialization complete");
  return true;
}

void start(void) {
  ULOG_INFO("Sound: Starting audio playback");

  if (!is_initialized) {
    ULOG_INFO("Sound: Not initialized, initializing first");
    init(NULL);
  }

  i2s6::enable();
  playing_state = true;
  ULOG_INFO("Sound: Audio playback started");
}

void stop(void) {
  ULOG_INFO("Sound: Stopping audio playback");
  i2s6::disable();
  playing_state = false;
  current_buffer = NULL;
  buffer_position = 0;
  ULOG_INFO("Sound: Audio playback stopped");
}

void set_volume(uint8_t volume) {
  if (volume > 100) volume = 100;
  ULOG_INFO("Sound: Setting volume from %u to %u", current_config.volume, volume);
  current_config.volume = volume;
}

void play_buffer(const audio_buffer_t* buffer) {
  if (buffer == NULL || buffer->data == NULL || buffer->length == 0) {
    ULOG_INFO("Sound: play_buffer called with invalid buffer");
    return;
  }

  ULOG_INFO("Sound: Playing buffer: length=%u samples, loop=%s, stereo=%s", buffer->length,
            buffer->loop ? "true" : "false", current_config.stereo ? "true" : "false");

  /* Ensure sound is initialized */
  if (!is_initialized) {
    ULOG_INFO("Sound: Not initialized, initializing first");
    init(NULL);
    start();
  }

  /* Set current buffer */
  current_buffer = buffer;
  buffer_position = 0;
  playing_state = true;

  ULOG_INFO("Sound: Starting buffer playback at position 0");

  /* Play the buffer (blocking) - use fast version for performance */
  while (buffer_position < buffer->length) {
    /* Get sample from buffer */
    int16_t sample = buffer->data[buffer_position];

    /* Apply volume */
    sample = samples::apply_volume(sample, current_config.volume);

    /* Send to I2S (stereo or mono) using fast version */
    if (current_config.stereo) {
      i2s6::send_sample_fast(sample, sample);
    } else {
      /* Mono: send to left channel only, right channel silent */
      i2s6::send_sample_fast(sample, 0);
    }

    buffer_position++;

    /* Check for loop */
    if (buffer_position >= buffer->length && buffer->loop) {
      ULOG_INFO("Sound: Buffer loop, resetting to position 0");
      buffer_position = 0;
    }
  }

  /* Reset playback state */
  current_buffer = NULL;
  buffer_position = 0;
  playing_state = false;

  ULOG_INFO("Sound: Buffer playback complete");
}

void beep(uint32_t frequency, uint32_t duration_ms, uint8_t volume) {
  if (frequency == 0 || duration_ms == 0) {
    ULOG_INFO("Sound: beep called with invalid parameters: frequency=%u, duration=%u", frequency, duration_ms);
    return;
  }

  ULOG_INFO("Sound: Playing beep: frequency=%u Hz, duration=%u ms, volume=%u", frequency, duration_ms, volume);

  /* Ensure sound is initialized */
  if (!is_initialized) {
    ULOG_INFO("Sound: Not initialized, initializing first");
    init(NULL);
    start();
  }

  /* Calculate number of samples */
  uint32_t num_samples = (current_config.sample_rate * duration_ms) / 1000;

  /* Calculate phase increment using 16.16 fixed-point arithmetic
   * phase_increment = (frequency * 64 * 65536) / sample_rate
   * This gives us the amount to add to phase each sample to achieve the desired frequency
   * The "64" is because we have a 64-entry sine table, and 65536 is for 16-bit fractional part
   */
  uint32_t phase = 0;
  uint32_t phase_increment = ((uint64_t)frequency * 64 * 65536) / current_config.sample_rate;

  ULOG_INFO("Sound: Generating %u samples, phase_increment=0x%08X", num_samples, phase_increment);

  /* Temporary volume override */
  uint8_t original_volume = current_config.volume;
  if (volume <= 100) {
    current_config.volume = volume;
    ULOG_INFO("Sound: Temporary volume override: %u -> %u", original_volume, current_config.volume);
  }

  /* Generate and play beep - use fast version for performance */
  for (uint32_t i = 0; i < num_samples; i++) {
    /* Generate sine wave sample using fixed-point phase */
    int16_t sample = samples::generate_sine_sample_fp(phase);

    /* Apply volume */
    sample = samples::apply_volume(sample, current_config.volume);

    /* Send to I2S using fast version */
    if (current_config.stereo) {
      i2s6::send_sample_fast(sample, sample);
    } else {
      i2s6::send_sample_fast(sample, 0);
    }

    /* Update phase (wraps automatically due to 32-bit overflow) */
    phase += phase_increment;
  }

  /* Restore original volume */
  current_config.volume = original_volume;
  ULOG_INFO("Sound: Beep complete, volume restored to %u", current_config.volume);
}

void test_tone(void) {
  ULOG_INFO("Sound: Playing test tone (440Hz, 1s, 90%% volume)");
  beep(440, 1000, 90); /* 440Hz for 1 second at 90% volume */
}

void error_beep(void) {
  ULOG_INFO("Sound: Playing error beep (1000Hz, 200ms, 90%% volume)");
  beep(1000, 200, 90); /* High-pitched short beep */
}

void success_beep(void) {
  ULOG_INFO("Sound: Playing success beep (300Hz, 300ms, 80%% volume)");
  beep(300, 300, 80); /* Low-pitched medium beep */
}

void warning_beep(void) {
  ULOG_INFO("Sound: Playing warning beep (two 600Hz, 150ms, 85%% volume)");
  /* Two short beeps */
  beep(600, 150, 85);

  /* Small delay between beeps */
  for (volatile uint32_t i = 0; i < 10000; i++)
    ; /* Simple delay */

  beep(600, 150, 85);
}

bool is_playing(void) {
  return playing_state;
}

void poll(void) {
  /* Simple polling implementation for background playback */
  if (!playing_state || current_buffer == NULL) {
    return;
  }

  /* Check if we have more samples to play */
  if (buffer_position >= current_buffer->length) {
    if (current_buffer->loop) {
      ULOG_INFO("Sound: poll - buffer loop, resetting to position 0");
      buffer_position = 0;
    } else {
      ULOG_INFO("Sound: poll - buffer playback complete");
      playing_state = false;
      current_buffer = NULL;
      buffer_position = 0;
      return;
    }
  }

  /* Get sample from buffer */
  int16_t sample = current_buffer->data[buffer_position];

  /* Apply volume */
  sample = samples::apply_volume(sample, current_config.volume);

  /* Send to I2S using fast version */
  if (current_config.stereo) {
    i2s6::send_sample_fast(sample, sample);
  } else {
    i2s6::send_sample_fast(sample, 0);
  }

  /* Log progress occasionally */
  if ((buffer_position % 1000) == 0) {
    ULOG_INFO("Sound: poll - playing sample %u/%u", buffer_position, current_buffer->length);
  }

  buffer_position++;
}

/**
 * @brief Generate simple test tone (440Hz, 1 second)
 */
void play_test_tone(void) {
  ULOG_INFO("Sound: play_test_tone called");
  test_tone(); /* Uses the existing 440Hz test tone */
}

/**
 * @brief Simple demonstration of sound driver
 */
void demo(void) {
  ULOG_INFO("Sound: Starting sound demo");

  /* Initialize sound driver */
  init(NULL);
  start();

  ULOG_INFO("Sound: Demo - Playing success beep");
  /* Play different beeps */
  success_beep();

  ULOG_INFO("Sound: Demo - Delay between beeps");
  /* Small delay */
  for (volatile uint32_t i = 0; i < 50000; i++)
    ;

  ULOG_INFO("Sound: Demo - Playing warning beep");
  warning_beep();

  ULOG_INFO("Sound: Demo - Delay between beeps");
  /* Small delay */
  for (volatile uint32_t i = 0; i < 50000; i++)
    ;

  ULOG_INFO("Sound: Demo - Playing error beep");
  error_beep();

  ULOG_INFO("Sound: Demo - Delay between beeps");
  /* Small delay */
  for (volatile uint32_t i = 0; i < 50000; i++)
    ;

  ULOG_INFO("Sound: Demo - Playing test tone");
  /* Play test tone */
  test_tone();

  ULOG_INFO("Sound: Demo complete");
}

}  // namespace xbot::driver::sound
