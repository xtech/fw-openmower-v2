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
 *   Priority levels (one FIFO queue, depth 4), driven by SoundDefinition::preempt:
 *     queued — the default; dropped when the queue is full.
 *     preempt — alerts (e.g. EMERGENCY) stops the running sound, drops everything
 *               queued and starts immediately.
 *
 *   The MAX98357A amplifier is hardware-wired for left-channel-only operation,
 *   so DMA frames are always [L_sample, 0].
 *
 *   MP3 requirements: 16 kHz mono (decoded by dr_mp3, no resampling).
 *   Sounds are resolved per SoundId from a SoundDefinition (see sound_definition.hpp):
 *     a flash override if present, else the ROM default.
 */

#ifndef SOUND_PLAYER_HPP
#define SOUND_PLAYER_HPP

#include <etl/string_view.h>

#include <SoundServiceBase.hpp>
#include <cstdint>

#include "sound_definition.hpp"  // SoundDefinition, kMaxPath (the variant list needs both)

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
};

/** @brief Short reason text for logs. */
inline const char* play_result_text(PlayResult result) {
  switch (result) {
    case PlayResult::QUEUED: return "queued";
    case PlayResult::QUEUE_FULL: return "REJECTED (queue full)";
    case PlayResult::INVALID_ARGUMENT: return "REJECTED (invalid argument)";
    case PlayResult::PLAYER_NOT_RUNNING: break;
  }
  return "REJECTED (player not running)";
}

/**
 * @brief Play a sound by logical identifier.
 *
 * Resolves the SoundDefinition for @p id: a flash override if present,
 * otherwise the ROM default (if exists).
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
 * @brief Register (or replace) a runtime override for one logical sound.
 *
 * Overrides are populated by the SoundService from the high-level
 * configuration blob and take precedence over the ROM defaults.  Use
 * clear_sound_overrides() to remove them all.
 *
 * @param id   Logical sound to override.
 * @param def  New definition (copied into RAM).
 */
void set_sound_override(SoundId id, const SoundDefinition& def);

/**
 * @brief Remove all runtime sound overrides, restoring the ROM defaults.
 */
void clear_sound_overrides();

/**
 * @brief Maximum number of takes stored for one sound.
 *
 * The high level may send several MP3s for one event (a spoken announcement with
 * different wording, or a sound with several recordings); the low level plays one
 * of them.
 */
constexpr uint8_t kMaxVariants = 8U;

/**
 * @brief Longest MP3 file name of one take.
 *
 * Only the name is stored; the "/sounds/" prefix is a low-level contract and is
 * prepended when the sound plays, so kMaxPath holds prefix + name + NUL.
 */
constexpr size_t kVariantNameMax = kMaxPath - sizeof("/sounds/") + 1U;

/** @brief How a sound with several takes chooses one (the "pick" of the definition). */
enum class PickMode : uint8_t {
  FIRST,   ///< Always take 1 - what a firmware without take support plays
  RANDOM,  ///< Any take (the default)
  ROTATE,  ///< Never the same take twice in a row
};

/*
 * Take lists are persisted per sound (the file format lives in sound_player.cpp)
 * and read back only when the sound plays: RAM keeps a 4-byte reference per sound
 * and a one-byte rotate state, the names (a few hundred bytes) come from LittleFS -
 * which is where the MP3 is streamed from anyway, so the read costs nothing next
 * to the audio.
 *
 * The SoundService writes the lists while it parses the definitions blob; nothing
 * is replaced before commit_sound_variants_write(), so a rejected blob leaves the
 * previous lists in place.
 */

/** @brief Start a fresh take store (staged; the current one stays until the commit). */
void begin_sound_variants_write();

/** @brief All add_sound_variant() calls that follow belong to @p id. */
void begin_sound_variant_set(SoundId id);

/**
 * @brief Append one take (an MP3 file name, no path) to the current set.
 * @return false on a name that is too long, more than kMaxVariants takes, or an I/O error.
 */
bool add_sound_variant(etl::string_view name);

/** @brief Close the current set; a set without takes is dropped. */
void end_sound_variant_set();

/** @brief Set the pick policy of @p id (in the blob it follows the take list). */
void set_sound_variant_pick(SoundId id, PickMode pick);

/** @brief Replace the store with the staged one and reload the index. */
void commit_sound_variants_write();

/** @brief Drop the staged store (the blob was rejected); the previous one stays. */
void abort_sound_variants_write();

/**
 * @brief Read the index of the persisted take store.
 *
 * Called at boot (player_init) and after a commit; a missing or mismatched file
 * (other magic/version/layout) is ignored with a warning, which simply means every
 * sound plays its first take.
 */
void load_sound_variants_from_storage();

/**
 * @brief Load persisted sound overrides from LittleFS (if any).
 *
 * Called at boot (player_init) so early sounds use the last-known
 * high-level definitions instead of the ROM defaults.
 */
void load_sound_overrides_from_storage();

/**
 * @brief Persist the current sound overrides to LittleFS.
 *
 * Called by the SoundService after a successful definitions-blob parse, so the
 * overrides survive a reboot.
 */
void save_sound_overrides_to_storage();

/**
 * @brief Stop the current playback and discard any queued sounds.
 *
 * The player becomes idle; subsequent play_*() calls start fresh.
 * Safe to call from any thread context.
 */
void stop();

/** @brief Returns true while the player is actively outputting audio. */
bool is_playing();

/**
 * @brief True with a probability of @p chance_percent (0..100).
 *
 * Uses the player's own PRNG (seeded from the MCU TRNG at boot, see random_seed()), so
 * the sound state machine rolls its `chance` per robot instead of repeating the same
 * pattern after every boot. Thread safe; costs a few instructions.
 */
bool random_chance(uint8_t chance_percent);

}  // namespace xbot::driver::sound

#endif  // SOUND_PLAYER_HPP
