/*
 * sound_driver.c - High-Level Sound Driver for STM32H723 with MAX98357
 *
 * This provides a simple API for audio playback using I2S6.
 *
 * Copyright (c) 2025 OpenMower Project
 */

#include "sound_driver.h"

#include <stddef.h>

#include "i2s6_lld.h"

/* Default configuration */
static const sound_config_t default_config = {
    .sample_rate = 44100,
    .volume = 80, /* 80% volume */
    .stereo = true,
};

/* Current configuration */
static sound_config_t current_config = default_config;

/* Current playback state */
static const audio_buffer_t* current_buffer = NULL;
static uint32_t buffer_position = 0;
static bool is_playing = false;
static bool is_initialized = false;

/* I2S6 configuration */
static i2s6_config_t i2s_config = {
    .sample_rate = 44100,
    .data_bits = 16,
    .channel_bits = 16,
    .master_mode = true,
    .i2s_standard = true,
    .i2s_mode = true,
};

/**
 * @brief Apply volume to sample
 */
static int16_t apply_volume(int16_t sample, uint8_t volume) {
  /* Simple volume scaling: sample * volume / 100 */
  int32_t scaled = (int32_t)sample * volume / 100;

  /* Clamp to 16-bit range */
  if (scaled > 32767) scaled = 32767;
  if (scaled < -32768) scaled = -32768;

  return (int16_t)scaled;
}

/**
 * @brief Generate sine wave sample
 */
static int16_t generate_sine_sample(uint32_t phase, uint32_t frequency, uint32_t sample_rate) {
  /* Simple sine approximation using fixed-point math */
  static const int16_t sine_table[] = {
      0,      3212,   6393,   9512,   12540,  15446,  18205,  20787,  23170,  25330,  27246,  28899,  30273,
      31357,  32138,  32610,  32767,  32610,  32138,  31357,  30273,  28899,  27246,  25330,  23170,  20787,
      18205,  15446,  12540,  9512,   6393,   3212,   0,      -3212,  -6393,  -9512,  -12540, -15446, -18205,
      -20787, -23170, -25330, -27246, -28899, -30273, -31357, -32138, -32610, -32767, -32610, -32138, -31357,
      -30273, -28899, -27246, -25330, -23170, -20787, -18205, -15446, -12540, -9512,  -6393,  -3212};

  /* Calculate phase increment per sample */
  uint32_t phase_increment = (frequency * 64) / sample_rate;
  if (phase_increment == 0) phase_increment = 1;

  /* Get phase index (0-63) */
  uint32_t phase_index = (phase / phase_increment) & 0x3F;

  return sine_table[phase_index];
}

bool sound_init(const sound_config_t* config) {
  if (config != NULL) {
    current_config = *config;
  } else {
    current_config = default_config;
  }

  /* Update I2S configuration */
  i2s_config.sample_rate = current_config.sample_rate;

  /* Initialize I2S6 */
  if (!i2s6_lld_init(&i2s_config)) {
    return false;
  }

  is_initialized = true;
  return true;
}

void sound_start(void) {
  if (!is_initialized) {
    sound_init(NULL);
  }

  i2s6_lld_enable();
  is_playing = true;
}

void sound_stop(void) {
  i2s6_lld_disable();
  is_playing = false;
  current_buffer = NULL;
  buffer_position = 0;
}

void sound_set_volume(uint8_t volume) {
  if (volume > 100) volume = 100;
  current_config.volume = volume;
}

void sound_play_buffer(const audio_buffer_t* buffer) {
  if (buffer == NULL || buffer->data == NULL || buffer->length == 0) {
    return;
  }

  /* Ensure sound is initialized */
  if (!is_initialized) {
    sound_init(NULL);
    sound_start();
  }

  /* Set current buffer */
  current_buffer = buffer;
  buffer_position = 0;
  is_playing = true;

  /* Play the buffer (blocking) */
  while (buffer_position < buffer->length) {
    /* Get sample from buffer */
    int16_t sample = buffer->data[buffer_position];

    /* Apply volume */
    sample = apply_volume(sample, current_config.volume);

    /* Send to I2S (stereo or mono) */
    if (current_config.stereo) {
      i2s6_lld_send_sample(sample, sample);
    } else {
      /* Mono: send to left channel only, right channel silent */
      i2s6_lld_send_sample(sample, 0);
    }

    buffer_position++;

    /* Check for loop */
    if (buffer_position >= buffer->length && buffer->loop) {
      buffer_position = 0;
    }
  }

  /* Reset playback state */
  current_buffer = NULL;
  buffer_position = 0;
  is_playing = false;
}

void sound_beep(uint32_t frequency, uint32_t duration_ms, uint8_t volume) {
  if (frequency == 0 || duration_ms == 0) {
    return;
  }

  /* Ensure sound is initialized */
  if (!is_initialized) {
    sound_init(NULL);
    sound_start();
  }

  /* Calculate number of samples */
  uint32_t num_samples = (current_config.sample_rate * duration_ms) / 1000;
  uint32_t phase = 0;
  uint32_t phase_increment = (frequency * 64) / current_config.sample_rate;
  if (phase_increment == 0) phase_increment = 1;

  /* Temporary volume override */
  uint8_t original_volume = current_config.volume;
  if (volume <= 100) {
    current_config.volume = volume;
  }

  /* Generate and play beep */
  for (uint32_t i = 0; i < num_samples; i++) {
    /* Generate sine wave sample */
    int16_t sample = generate_sine_sample(phase, frequency, current_config.sample_rate);

    /* Apply volume */
    sample = apply_volume(sample, current_config.volume);

    /* Send to I2S */
    if (current_config.stereo) {
      i2s6_lld_send_sample(sample, sample);
    } else {
      i2s6_lld_send_sample(sample, 0);
    }

    /* Update phase */
    phase += phase_increment;
    if (phase >= (current_config.sample_rate * 64 / frequency)) {
      phase = 0;
    }
  }

  /* Restore original volume */
  current_config.volume = original_volume;
}

void sound_test_tone(void) {
  sound_beep(440, 1000, 90); /* 440Hz for 1 second at 90% volume */
}

void sound_error_beep(void) {
  sound_beep(1000, 200, 90); /* High-pitched short beep */
}

void sound_success_beep(void) {
  sound_beep(300, 300, 80); /* Low-pitched medium beep */
}

void sound_warning_beep(void) {
  /* Two short beeps */
  sound_beep(600, 150, 85);

  /* Small delay between beeps */
  for (volatile uint32_t i = 0; i < 10000; i++)
    ; /* Simple delay */

  sound_beep(600, 150, 85);
}

bool sound_is_playing(void) {
  return is_playing;
}

void sound_poll(void) {
  /* Simple polling implementation for background playback */
  if (!is_playing || current_buffer == NULL) {
    return;
  }

  /* Check if we have more samples to play */
  if (buffer_position >= current_buffer->length) {
    if (current_buffer->loop) {
      buffer_position = 0;
    } else {
      is_playing = false;
      current_buffer = NULL;
      buffer_position = 0;
      return;
    }
  }

  /* Get sample from buffer */
  int16_t sample = current_buffer->data[buffer_position];

  /* Apply volume */
  sample = apply_volume(sample, current_config.volume);

  /* Send to I2S */
  if (current_config.stereo) {
    i2s6_lld_send_sample(sample, sample);
  } else {
    i2s6_lld_send_sample(sample, 0);
  }

  buffer_position++;
}

/**
 * @brief Generate simple test tone (440Hz, 1 second)
 */
void sound_play_test_tone(void) {
  sound_test_tone(); /* Uses the existing 440Hz test tone */
}

/**
 * @brief Simple demonstration of sound driver
 */
void sound_demo(void) {
  /* Initialize sound driver */
  sound_init(NULL);
  sound_start();

  /* Play different beeps */
  sound_success_beep();

  /* Small delay */
  for (volatile uint32_t i = 0; i < 50000; i++)
    ;

  sound_warning_beep();

  /* Small delay */
  for (volatile uint32_t i = 0; i < 50000; i++)
    ;

  sound_error_beep();

  /* Small delay */
  for (volatile uint32_t i = 0; i < 50000; i++)
    ;

  /* Play test tone */
  sound_test_tone();
}
