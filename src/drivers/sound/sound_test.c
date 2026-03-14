/*
 * sound_test.c - Test program for STM32H723 Sound Driver with MAX98357
 *
 * This provides a simple test to verify I2S6 audio output.
 *
 * Copyright (c) 2025 OpenMower Project
 */

#include <stddef.h>

#include "sound_driver.h"

/**
 * @brief Simple test using polling mode
 *
 * This function demonstrates basic audio playback.
 * It should be called from the main loop.
 */
void sound_test_polling(void) {
  static uint32_t test_state = 0;
  static uint32_t counter = 0;

  /* Initialize on first call */
  if (test_state == 0) {
    sound_init(NULL);
    sound_start();
    test_state = 1;
    counter = 0;
  }

  /* Simple state machine for test sequence */
  switch (test_state) {
    case 1:
      /* Play success beep once */
      sound_success_beep();
      test_state = 2;
      counter = 0;
      break;

    case 2:
      /* Wait a bit */
      counter++;
      if (counter > 100000) {
        test_state = 3;
        counter = 0;
      }
      break;

    case 3:
      /* Play warning beep */
      sound_warning_beep();
      test_state = 4;
      counter = 0;
      break;

    case 4:
      /* Wait a bit */
      counter++;
      if (counter > 100000) {
        test_state = 5;
        counter = 0;
      }
      break;

    case 5:
      /* Play error beep */
      sound_error_beep();
      test_state = 6;
      counter = 0;
      break;

    case 6:
      /* Wait a bit */
      counter++;
      if (counter > 200000) {
        test_state = 7;
        counter = 0;
      }
      break;

    case 7:
      /* Play 440Hz test tone */
      sound_test_tone();
      test_state = 8;
      counter = 0;
      break;

    case 8:
      /* Wait for tone to finish and stop */
      counter++;
      if (counter > 500000) {
        sound_stop();
        test_state = 0; /* Reset for next run */
      }
      break;

    default: test_state = 0; break;
  }
}

/**
 * @brief Continuous test tone (for debugging)
 *
 * This function plays a continuous 440Hz tone.
 * Useful for verifying hardware connection.
 */
void sound_test_continuous_tone(void) {
  static bool initialized = false;

  if (!initialized) {
    sound_init(NULL);
    sound_start();
    initialized = true;
  }

  /* Simple 440Hz tone using the low-level driver directly */
  static uint32_t phase = 0;
  const uint32_t frequency = 440;
  const uint32_t sample_rate = 44100;

  /* Simple sine wave generation */
  static const int16_t sine_table[] = {
      0,      3212,   6393,   9512,   12540,  15446,  18205,  20787,  23170,  25330,  27246,  28899,  30273,
      31357,  32138,  32610,  32767,  32610,  32138,  31357,  30273,  28899,  27246,  25330,  23170,  20787,
      18205,  15446,  12540,  9512,   6393,   3212,   0,      -3212,  -6393,  -9512,  -12540, -15446, -18205,
      -20787, -23170, -25330, -27246, -28899, -30273, -31357, -32138, -32610, -32767, -32610, -32138, -31357,
      -30273, -28899, -27246, -25330, -23170, -20787, -18205, -15446, -12540, -9512,  -6393,  -3212};

  /* Calculate phase increment */
  uint32_t phase_increment = (frequency * 64) / sample_rate;
  if (phase_increment == 0) phase_increment = 1;

  /* Get sample from table */
  uint32_t phase_index = (phase / phase_increment) & 0x3F;
  int16_t sample = sine_table[phase_index];

  /* Send to I2S (both channels) */
  extern void i2s6_lld_send_sample(int16_t left, int16_t right);
  i2s6_lld_send_sample(sample, sample);

  /* Update phase */
  phase++;
  if (phase >= (sample_rate * 64 / frequency)) {
    phase = 0;
  }
}

/**
 * @brief Simple beep test
 *
 * Plays a short beep at specified frequency.
 * Useful for quick hardware verification.
 */
void sound_test_simple_beep(uint32_t frequency) {
  sound_init(NULL);
  sound_start();
  sound_beep(frequency, 500, 70); /* 500ms beep at 70% volume */
  sound_stop();
}

/**
 * @brief Volume sweep test
 *
 * Plays a tone while sweeping volume from 0% to 100%.
 */
void sound_test_volume_sweep(void) {
  sound_init(NULL);
  sound_start();

  const uint32_t frequency = 440;
  const uint32_t duration_per_step = 100; /* 100ms per volume step */

  /* Sweep volume up */
  for (uint8_t volume = 10; volume <= 100; volume += 10) {
    sound_set_volume(volume);
    sound_beep(frequency, duration_per_step, volume);
  }

  /* Sweep volume down */
  for (uint8_t volume = 100; volume >= 10; volume -= 10) {
    sound_set_volume(volume);
    sound_beep(frequency, duration_per_step, volume);
  }

  sound_stop();
}

/**
 * @brief Frequency sweep test
 *
 * Plays a sweep from low to high frequency.
 */
void sound_test_frequency_sweep(void) {
  sound_init(NULL);
  sound_start();

  const uint32_t start_freq = 100;
  const uint32_t end_freq = 1000;
  const uint32_t step = 50;
  const uint32_t duration_per_step = 50; /* 50ms per frequency */

  /* Sweep up */
  for (uint32_t freq = start_freq; freq <= end_freq; freq += step) {
    sound_beep(freq, duration_per_step, 70);
  }

  /* Sweep down */
  for (uint32_t freq = end_freq; freq >= start_freq; freq -= step) {
    sound_beep(freq, duration_per_step, 70);
  }

  sound_stop();
}

/**
 * @brief Main test function
 *
 * Runs all tests in sequence.
 * Call this from main() for comprehensive testing.
 */
void sound_run_all_tests(void) {
  /* Test 1: Simple beeps */
  sound_test_simple_beep(440);

  /* Small delay */
  for (volatile uint32_t i = 0; i < 100000; i++)
    ;

  /* Test 2: Volume sweep */
  sound_test_volume_sweep();

  /* Small delay */
  for (volatile uint32_t i = 0; i < 100000; i++)
    ;

  /* Test 3: Frequency sweep */
  sound_test_frequency_sweep();

  /* Small delay */
  for (volatile uint32_t i = 0; i < 100000; i++)
    ;

  /* Test 4: Demo sequence */
  sound_demo();
}
