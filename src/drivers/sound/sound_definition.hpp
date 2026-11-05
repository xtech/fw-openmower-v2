/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sound_definition.hpp
 * @brief Serialisable sound definitions and the ROM default table.
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-03-23
 *
 * @note  A SoundDefinition describes what a given robot event sounds like:
 *        a simple tone, a note sequence, or a 16 kHz mono MP3 file.  The
 *        struct is flat and pointer-free so the very same
 *        type can be used both as a constexpr ROM default and as a raw blob
 *        stored in flash (LittleFS) — flash overrides take precedence over the
 *        ROM defaults.
 *
 *        ROM defaults are tone/sequence only; MP3 files live in flash overrides
 *        (uploaded by the high-level system or a future Sound-CLI).
 *
 *        TODO(HL): MP3 sounds must be 16 kHz mono. The high-level upload
 *        path must verify (and downsample if needed) before uploading — the
 *        firmware decodes at 16 kHz only and does NOT resample.
 */

#ifndef SOUND_DEFINITION_HPP
#define SOUND_DEFINITION_HPP

#include <SoundServiceBase.hpp>
#include <cstddef>
#include <cstdint>

#include "sound_synth.hpp"

namespace xbot::driver::sound {

/**
 * @brief Maximum length of the MP3 path stored in a FILE definition.
 *
 * The firmware prefixes "/sounds/", so this must also hold that prefix,
 * the file name and the terminating NUL:
 */
constexpr size_t kMaxPath = 64U;

/**
 * @brief A complete, self-contained sound definition.
 */
struct SoundDefinition {
  SoundType type;
  uint8_t volume;                      ///< Per-definition volume (0–100); master volume applies on top
  Waveform waveform = Waveform::SINE;  ///< Oscillator waveform (tone/sequence only)
  uint8_t unison = 1U;                 ///< Number of detuned voices (1 = single, odd: 3/5/7)
  uint16_t detune_hz = 0U;             ///< Frequency spread between unison voices in Hz
  uint16_t repeat_ms = 0U;             ///< Repeat the sound every n ms until another sound plays
  uint16_t pause_after_ms = 0U;        ///< Silence after this sound before the next one starts (0 = play through)
  union alignas(4) {
    struct {
      uint16_t freq;
      uint16_t duration_ms;
    } tone;
    struct {
      Note notes[kMaxNotes];
      uint8_t count;
      uint8_t attack_ms;  ///< Linear attack ramp in ms, 0..255 (0 = instant onset)
      uint8_t decay_ms;   ///< Exponential fade to ~-60 dB in ms, 0..255 (0 = hold the note)
    } sequence;
    char path[kMaxPath];  ///< MP3 file path (16 kHz mono)
  };
};

/*---------------------------------------------------------------------------
 * ROM defaults — tone/sequence only, indexed by SoundId.
 *
 * ⚠️  The table is POSITIONAL and covers a PREFIX of the ids: entry n is the default
 *     for SoundId n, so its order has to match "SoundId" in services/sound_service.json.
 *     Ids from kDefaultSoundDefsCount on have no ROM sound: play_sound_id() accepts them
 *     and stays silent until the high level provides a definition for them.
 *---------------------------------------------------------------------------*/

inline constexpr SoundDefinition kDefaultSoundDefs[] = {
    // BOOT_PING - the player repeats it (repeat_ms) while Stage 2 waits for ROS.
    {SoundType::SEQUENCE, 30, .waveform = Waveform::SINE, .repeat_ms = 2000,
     .sequence = {{{554, 360, 0, 0}}, 1, 4, 220}},  // BOOT_PING
    // BOOT_COMPLETE
    {SoundType::SEQUENCE, 85, .waveform = Waveform::TRIANGLE, .unison = 3, .detune_hz = 6,
     .sequence = {{{262, 90, 0, 0}, {330, 90, 0, 0}, {392, 90, 0, 0}, {523, 300, 0, 0}}, 4, 0, 0}},
    // SUCCESS - a short rising figure (D6 D6 G6), deliberately distinct from the 554 Hz decaying ping of BOOT_PING.
    {SoundType::SEQUENCE, 65, .waveform = Waveform::TRIANGLE,
     .sequence = {{{1174, 60, 0, 0}, {0, 40, 0, 0}, {1174, 60, 0, 0}, {0, 40, 0, 0}, {1568, 140, 0, 0}}, 5, 0, 80}},
    // WARNING
    {SoundType::SEQUENCE, 80, .waveform = Waveform::SAW,
     .sequence = {{{880, 150, 0, 0}, {0, 80, 0, 0}, {880, 150, 0, 0}, {0, 80, 0, 0}}, 4, 0, 0}},
    // ERROR
    {SoundType::SEQUENCE, 85, .waveform = Waveform::SAW,
     .sequence = {{{300, 200, 0, 0}, {240, 200, 0, 0}, {180, 300, 0, 0}}, 3, 0, 0}},
    // EMERGENCY
    {SoundType::SEQUENCE, 90, .sequence = {{{950, 8000, 20, 220}}, 1, 0, 0}},
    // EMERGENCY_CLEARED
    {SoundType::SEQUENCE, 65, .waveform = Waveform::TRIANGLE,
     .sequence = {{{1174, 60, 0, 0}, {0, 40, 0, 0}, {1174, 60, 0, 0}, {0, 40, 0, 0}, {1568, 140, 0, 0}}, 5, 0, 80}},
    // ROS_CONNECTED - the high level (ROS) link came up (see EmergencyService::CheckTimeouts)
    {SoundType::SEQUENCE, 80, .waveform = Waveform::SINE, .unison = 3, .detune_hz = 4,
     .sequence = {{{880, 120, 0, 0}, {1174, 180, 0, 0}}, 2, 4, 120}},
    // ROS_DISCONNECTED - heartbeat timeout, i.e. the high level is gone
    {SoundType::SEQUENCE, 80, .waveform = Waveform::SAW, .sequence = {{{880, 120, 0, 0}, {587, 220, 0, 0}}, 2, 4, 150}},
    // GPS_RTK_FIX
    {SoundType::SEQUENCE, 80, .waveform = Waveform::SINE, .unison = 3, .detune_hz = 4,
     .sequence = {{{880, 120, 0, 0}, {1174, 180, 0, 0}}, 2, 0, 0}},
    // GPS_RTK_LOST
    {SoundType::SEQUENCE, 80, .waveform = Waveform::SAW, .sequence = {{{660, 120, 0, 0}, {440, 200, 0, 0}}, 2, 0, 0}},
    // AUTONOMOUS_NOTICE - the reversing alarm of lorries and forklifts
    {SoundType::SEQUENCE, 75, .waveform = Waveform::SINE, .repeat_ms = 5000,
     .sequence = {{{970, 350, 0, 0}, {0, 650, 0, 0}, {970, 350, 0, 0}, {0, 650, 0, 0}, {970, 350, 0, 0}}, 5, 0, 0}},
    // VOLUME_UP - a short rising figure: "one step louder" (stays useful without configuration)
    {SoundType::SEQUENCE, 60, .waveform = Waveform::SINE,
     .sequence = {{{660, 70, 0, 0}, {880, 70, 0, 0}, {1100, 90, 0, 0}}, 3, 3, 60}},
    // VOLUME_DOWN - the same figure falling: "one step quieter"
    {SoundType::SEQUENCE, 60, .waveform = Waveform::SINE,
     .sequence = {{{1100, 70, 0, 0}, {880, 70, 0, 0}, {660, 90, 0, 0}}, 3, 3, 60}},
    // LOW_BATTERY
    {SoundType::SEQUENCE, 60,
     .sequence = {{{659, 300, 0, 0}, {587, 300, 0, 0}, {523, 300, 0, 0}, {440, 400, 0, 0}}, 4, 0, 0}},
    // CHARGING_START
    {SoundType::SEQUENCE, 60, .waveform = Waveform::SINE,
     .sequence = {{{523, 100, 0, 0}, {659, 100, 0, 0}, {784, 150, 0, 0}}, 3, 0, 0}},
    // CHARGING_DONE
    {SoundType::SEQUENCE, 60, .waveform = Waveform::TRIANGLE, .unison = 3, .detune_hz = 6,
     .sequence = {{{523, 120, 0, 0}, {659, 120, 0, 0}, {784, 120, 0, 0}, {1046, 300, 0, 0}}, 4, 0, 0}},
};

/** @brief How many ids the ROM default table covers (SoundId 0 .. count-1). */
inline constexpr size_t kDefaultSoundDefsCount = sizeof(kDefaultSoundDefs) / sizeof(kDefaultSoundDefs[0]);

static_assert(sizeof(SoundDefinition) == 80U, "SoundDefinition size changed — it is part of the flash format");

}  // namespace xbot::driver::sound

#endif  // SOUND_DEFINITION_HPP
