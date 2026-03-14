/*
 * sound_driver.h - High-Level Sound Driver for STM32H723 with MAX98357
 *
 * This provides a simple API for audio playback using I2S6.
 *
 * Copyright (c) 2025 OpenMower Project
 */

#ifndef SOUND_DRIVER_H
#define SOUND_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sound driver configuration
 */
typedef struct {
  uint32_t sample_rate; /* Sample rate in Hz (default: 44100) */
  uint8_t volume;       /* Volume level (0-100) */
  bool stereo;          /* true = stereo, false = mono */
} sound_config_t;

/**
 * @brief Audio buffer structure
 */
typedef struct {
  const int16_t* data; /* Pointer to audio data */
  uint32_t length;     /* Number of samples (per channel) */
  bool loop;           /* true = loop playback */
} audio_buffer_t;

/**
 * @brief Initialize sound driver
 *
 * @param config Configuration (NULL for defaults)
 * @return true if successful, false otherwise
 */
bool sound_init(const sound_config_t* config);

/**
 * @brief Start audio playback
 */
void sound_start(void);

/**
 * @brief Stop audio playback
 */
void sound_stop(void);

/**
 * @brief Set volume level
 *
 * @param volume Volume level (0-100)
 */
void sound_set_volume(uint8_t volume);

/**
 * @brief Play audio buffer
 *
 * @param buffer Audio buffer to play
 * @note This is a blocking call for simple polling mode
 */
void sound_play_buffer(const audio_buffer_t* buffer);

/**
 * @brief Play simple beep
 *
 * @param frequency Frequency in Hz
 * @param duration_ms Duration in milliseconds
 * @param volume Volume (0-100)
 */
void sound_beep(uint32_t frequency, uint32_t duration_ms, uint8_t volume);

/**
 * @brief Play test tone (440Hz)
 */
void sound_test_tone(void);

/**
 * @brief Play error beep (short high-pitched beep)
 */
void sound_error_beep(void);

/**
 * @brief Play success beep (short low-pitched beep)
 */
void sound_success_beep(void);

/**
 * @brief Play warning beep (two short beeps)
 */
void sound_warning_beep(void);

/**
 * @brief Check if sound is currently playing
 *
 * @return true if playing, false if idle
 */
bool sound_is_playing(void);

/**
 * @brief Simple polling-based audio playback handler
 *
 * This should be called in the main loop for continuous playback
 */
void sound_poll(void);

/**
 * @brief Simple demonstration of sound driver
 *
 * Plays a sequence of beeps and a test tone
 */
void sound_demo(void);

#ifdef __cplusplus
}
#endif

#endif /* SOUND_DRIVER_H */
