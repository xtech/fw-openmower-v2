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
 * @note  Audio is driven by a dedicated ChibiOS thread that owns the I2S6 / BDMA peripheral.
 *   Callers enqueue sound requests via play_sound_id(), play_tone(), or play_file();
 *   the player thread streams PCM to BDMA.
 *
 *   The MAX98357A amplifier is hardware-wired for left-channel-only operation,
 *   so DMA frames are always [L_sample, 0].
 *
 *   MP3 requirements: 16 kHz mono (decoded by dr_mp3, no resampling).
 *   Sounds are resolved per SoundId from a SoundDefinition (see sound_definition.hpp):
 *     a stored override if present, else the ROM default.
 */

#ifndef SOUND_PLAYER_HPP
#define SOUND_PLAYER_HPP

#include <SoundServiceBase.hpp>
#include <cstdint>

#include "sound_definition.hpp"  // Note/Waveform (via sound_synth.hpp)
#include "sound_random.hpp"
#include "sound_storage.hpp"
#include "sound_variants.hpp"

namespace xbot::driver::sound {

struct Note;  ///< Defined in sound_synth.hpp.

/**
 * @brief Initialise the sound player and start the player thread.
 *
 * Calls i2sStart(&I2SD6) once; must be called after halInit() / chSysInit()
 * and after the LittleFS filesystem has been mounted.
 */
void player_init();

/**
 * @brief Result of a play request.
 *
 * Carried back to the caller so a host (or the RPC log) can tell *why* something was not played
 */
enum class PlayResult : uint8_t {
  QUEUED,              ///< Accepted: queued (or preempting the current sound)
  PLAYER_NOT_RUNNING,  ///< The player thread is not started (no sound hardware / too early)
  QUEUE_FULL,          ///< Dropped: queue is full and this request does not preempt
  INVALID_ARGUMENT,    ///< Dropped: nothing to play (zero freq/duration, no notes, null path)
  NO_DEFINITION,       ///< Accepted, but silent: this SoundId has neither an override nor a ROM default
};

/** @brief Short reason text for logs. */
inline const char* play_result_text(PlayResult result) {
  switch (result) {
    case PlayResult::QUEUED: return "queued";
    case PlayResult::QUEUE_FULL: return "REJECTED (queue full)";
    case PlayResult::INVALID_ARGUMENT: return "REJECTED (invalid argument)";
    case PlayResult::NO_DEFINITION: return "silent (no definition configured)";
    case PlayResult::PLAYER_NOT_RUNNING: break;
  }
  return "REJECTED (player not running)";
}

/** @brief True when the request was accepted (QUEUED, or accepted but silent). */
inline bool play_result_accepted(PlayResult result) {
  return result == PlayResult::QUEUED || result == PlayResult::NO_DEFINITION;
}

/**
 * @brief Play a sound by logical identifier.
 *
 * Resolves the SoundDefinition for @p id: a flash override if present,
 * otherwise the ROM default (if exists). A SoundId without either is accepted but silent
 * (PlayResult::NO_DEFINITION).
 *
 * @param id  Logical sound to play.
 * @return QUEUED when accepted, otherwise why it was rejected (see PlayResult)
 */
PlayResult play_sound_id(SoundId id);

/**
 * @brief Play a synthesised sine-wave tone.
 *
 * @param freq          Frequency in Hz
 * @param duration_ms   Duration in milliseconds
 * @param volume        Volume (0–100)
 * @param preempt       true: stop the running sound and drop the queue
 * @return QUEUED when accepted, otherwise why it was rejected (see PlayResult)
 */
PlayResult play_tone(uint16_t freq, uint16_t duration_ms, uint8_t volume = 80, bool preempt = false);

/**
 * @brief Play an MP3 file from LittleFS.
 *
 * @param path          Absolute path to a 16 kHz / mono MP3 file
 * @param preempt       true: stop the running sound and drop the queue
 * @return QUEUED when accepted, otherwise why it was rejected (see PlayResult)
 */
PlayResult play_file(const char* path, bool preempt = false);

/**
 * @brief Play an ad-hoc note sequence (used by the SoundService RPCs).
 *
 * Builds a SoundType::SEQUENCE request so a host can audition a sequence at
 * runtime, e.g. "250:60 0:40 375:80" (see sound_sequence.hpp).
 *
 * @param notes         Note array with at least @p count entries
 * @param count         Number of notes (clamped to kMaxNotes; 0 is ignored)
 * @param waveform      Oscillator waveform
 * @param volume        Per-definition volume (0–100)
 * @param unison        Detuned voices (1 = single, odd: 3/5/7)
 * @param detune_hz     Frequency spread between unison voices in Hz
 * @param attack_ms     Per-note attack ramp (0–255 ms, 0 = instant onset)
 * @param decay_ms      Per-note fade to ~-60 dB (0–255 ms, 0 = hold the note)
 * @param repeat_ms     Repeat the sequence every n ms until another sound plays (0 = play once).
 * @param preempt       true: stop the running sound and drop the queue
 * @return QUEUED when accepted, otherwise why it was rejected (see PlayResult)
 */
PlayResult play_sequence(const Note* notes, uint8_t count, Waveform waveform, uint8_t volume = 80, uint8_t unison = 1U,
                         uint16_t detune_hz = 0U, uint8_t attack_ms = 0U, uint8_t decay_ms = 0U,
                         uint16_t repeat_ms = 0U, bool preempt = false);

/**
 * @brief Set the master playback volume.
 *
 * Scales every sound (tone, sequence and WAV) on top of its own per-type
 * volume; takes effect on the next half-buffer fill.
 *
 * @param volume 0–100.
 */
void set_volume(uint8_t volume);

/**
 * @brief Stop the current playback and discard any queued sounds.
 *
 * The player becomes idle; subsequent play_*() calls start fresh.
 * Safe to call from any thread context.
 */
void stop();

/** @brief Returns true while the player is actively outputting audio. */
bool is_playing();

}  // namespace xbot::driver::sound

#endif  // SOUND_PLAYER_HPP
