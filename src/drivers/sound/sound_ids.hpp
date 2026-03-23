/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sound_ids.hpp
 * @brief Sound identifiers, note sequences, and fallback synthesis table.
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-03-23
 *
 * @note  The WAV file for a given SoundId lives at /sounds/<id>.wav on LittleFS.
 *        If absent, the player synthesises the corresponding entry from kFallbackTones[].
 *
 *        Fallback sounds are note sequences rendered by the player's SEQUENCE source:
 *          - Multi-note arpeggios / melodies for structured sounds
 *          - LFO frequency modulation for the emergency siren (smooth sweep, not steps)
 *
 *        All data lives in .rodata (constexpr) — zero RAM, ~250 bytes flash.
 */

#ifndef SOUND_IDS_HPP
#define SOUND_IDS_HPP

#include <cstdint>

namespace xbot::driver::sound {

/** @brief Logical sound identifiers. */
enum class SoundId : uint8_t {
  BOOT,            ///< Boot sequence complete
  SUCCESS,         ///< Operation succeeded
  WARNING,         ///< Non-critical warning
  ERROR,           ///< Recoverable error
  EMERGENCY,       ///< Emergency stop triggered
  LOW_BATTERY,     ///< Battery low
  CHARGING_START,  ///< Charging begun
  CHARGING_DONE,   ///< Charging complete
  COUNT            ///< Sentinel — number of defined sound IDs
};

/**
 * @brief A single note in a fallback sound sequence.
 *
 * When @p lfo_hz_x10 is non-zero the player applies sinusoidal frequency modulation
 * to the note, sweeping the instantaneous frequency between
 * @p freq – @p lfo_depth … @p freq + @p lfo_depth Hz at @p lfo_hz_x10 / 10 Hz.
 */
struct Note {
  uint16_t freq;         ///< Fundamental frequency in Hz; 0 = silence/pause
  uint16_t duration_ms;  ///< Note duration in milliseconds
  uint16_t lfo_hz_x10;   ///< LFO rate × 10  (e.g. 20 = 2.0 Hz); 0 = disabled
  uint16_t lfo_depth;    ///< Frequency deviation in Hz (ignored when lfo_hz_x10 == 0)
};

/** @brief A named fallback sound: pointer to a .rodata Note array + count + volume. */
struct SoundFallback {
  const Note* notes;  ///< Points into .rodata — safe to store as pointer in SoundRequest
  uint8_t count;
  uint8_t volume;  ///< Master volume for the sequence (0–100)
};

/*---------------------------------------------------------------------------
 * Note sequences — all constexpr, all in .rodata.
 *---------------------------------------------------------------------------*/

/** @brief BOOT — gentle two-note ascending interval. */
inline constexpr Note kBootNotes[] = {
    {440, 200, 0, 0}, /* A4 */
    {660, 350, 0, 0}, /* E5 */
};

/** @brief SUCCESS — C–E–G ascending major arpeggio. */
inline constexpr Note kSuccessNotes[] = {
    {523, 90, 0, 0},  /* C5 */
    {659, 90, 0, 0},  /* E5 */
    {784, 250, 0, 0}, /* G5 */
};

/** @brief WARNING — double beep with short pause between. */
inline constexpr Note kWarningNotes[] = {
    {880, 150, 0, 0}, /* A5 */
    {0, 50, 0, 0},    /* silence */
    {880, 150, 0, 0}, /* A5 */
};

/** @brief ERROR — descending Bb→F→A dissonant riff ("wrong answer"). */
inline constexpr Note kErrorNotes[] = {
    {466, 140, 0, 0}, /* Bb4 */
    {349, 140, 0, 0}, /* F4  */
    {220, 320, 0, 0}, /* A3  */
};

/**
 * @brief EMERGENCY — smooth siren sweep.
 *
 * Center 950 Hz, LFO 2.0 Hz, depth ±220 Hz → sweeps 730–1170 Hz continuously.
 * Single long note; the caller can stop it via stop() or preempt with HIGH priority.
 */
inline constexpr Note kEmergencyNotes[] = {
    {950, 8000, 20, 220},
};

/** @brief LOW_BATTERY — slow descending three-note sequence with pauses. */
inline constexpr Note kLowBattNotes[] = {
    {523, 250, 0, 0}, /* C5  */
    {0, 100, 0, 0},   /* pause */
    {392, 250, 0, 0}, /* G4  */
    {0, 100, 0, 0},   /* pause */
    {261, 450, 0, 0}, /* C4  */
};

/** @brief CHARGING_START — quick ascending three-note jingle. */
inline constexpr Note kChgStartNotes[] = {
    {440, 80, 0, 0}, /* A4  */
    {554, 80, 0, 0}, /* C#5 */
    {659, 80, 0, 0}, /* E5  */
};

/** @brief CHARGING_DONE — satisfying ascending four-note arrival. */
inline constexpr Note kChgDoneNotes[] = {
    {392, 90, 0, 0},  /* G4  */
    {523, 90, 0, 0},  /* C5  */
    {659, 90, 0, 0},  /* E5  */
    {784, 300, 0, 0}, /* G5  */
};

/*---------------------------------------------------------------------------
 * Fallback table — indexed by SoundId, entries must match enumerator order.
 *---------------------------------------------------------------------------*/

inline constexpr SoundFallback kFallbackTones[] = {
    /* BOOT           */ {kBootNotes, 2, 70},
    /* SUCCESS        */ {kSuccessNotes, 3, 75},
    /* WARNING        */ {kWarningNotes, 3, 80},
    /* ERROR          */ {kErrorNotes, 3, 85},
    /* EMERGENCY      */ {kEmergencyNotes, 1, 90},
    /* LOW_BATTERY    */ {kLowBattNotes, 5, 75},
    /* CHARGING_START */ {kChgStartNotes, 3, 65},
    /* CHARGING_DONE  */ {kChgDoneNotes, 4, 70},
};

static_assert(sizeof(kFallbackTones) / sizeof(kFallbackTones[0]) == static_cast<uint8_t>(SoundId::COUNT),
              "kFallbackTones must have one entry per SoundId");

}  // namespace xbot::driver::sound

#endif  // SOUND_IDS_HPP
