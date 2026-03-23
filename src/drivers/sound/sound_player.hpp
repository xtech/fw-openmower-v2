/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sound_player.hpp
 * @brief Event-driven sound player for STM32H723 with MAX98357A.
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-03-23
 *
 * @note  Audio is driven by a dedicated ChibiOS thread that owns the I2S6 / BDMA
 *        peripheral.  Callers enqueue sound requests via play_sound_id(),
 *        play_tone(), or play_file(); the player thread streams PCM to BDMA.
 *
 *        Priority levels:
 *          HIGH   — preempts whatever is currently playing (depth 1).
 *          NORMAL — queued in FIFO order, dropped when the queue is full (depth 4).
 *
 *        The MAX98357A amplifier is hardware-wired for left-channel-only operation,
 *        so DMA frames are always [L_sample, 0].
 *
 *        WAV requirements: 16 kHz, 16-bit, mono PCM (RIFF/PCM, canonical 44-byte header).
 *        Files must reside at /sounds/<SoundId>.wav on LittleFS.  When absent the
 *        player falls back to an on-the-fly synthesised tone from kFallbackTones[].
 */

#ifndef SOUND_PLAYER_HPP
#define SOUND_PLAYER_HPP

#include <cstdint>

#include "sound_ids.hpp"

namespace xbot::driver::sound {

/**
 * @brief Initialise the sound player and start the player thread.
 *
 * Calls i2sStart(&I2SD6) once; must be called after halInit() / chSysInit()
 * and after the LittleFS filesystem has been mounted.
 * Calling more than once has no effect.
 */
void player_init();

/**
 * @brief Play a sound by logical identifier.
 *
 * Looks for /sounds/<id>.wav on LittleFS; falls back to kFallbackTones[id]
 * if the file does not exist.
 *
 * @param id            Logical sound to play.
 * @param high_priority If true the sound preempts current playback immediately.
 */
void play_sound_id(SoundId id, bool high_priority = false);

/**
 * @brief Play a synthesised sine-wave tone.
 *
 * @param freq          Frequency in Hz.
 * @param duration_ms   Duration in milliseconds.
 * @param volume        Volume (0–100).
 * @param high_priority If true the tone preempts current playback immediately.
 */
void play_tone(uint32_t freq, uint32_t duration_ms, uint8_t volume = 80, bool high_priority = false);

/**
 * @brief Play a WAV file from LittleFS.
 *
 * @param path          Absolute path to a 16 kHz / 16-bit / mono PCM WAV file.
 * @param high_priority If true the file preempts current playback immediately.
 */
void play_file(const char* path, bool high_priority = false);

/**
 * @brief Set the master playback volume.
 *
 * Applied per-sample in the fill loop; takes effect on the next queued sound.
 *
 * @param volume 0–100.
 */
void set_volume(uint8_t volume);

/** @brief Returns true while the player is actively outputting audio. */
bool is_playing();

}  // namespace xbot::driver::sound

#endif  // SOUND_PLAYER_HPP
