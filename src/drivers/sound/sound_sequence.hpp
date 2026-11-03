/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sound_sequence.hpp
 * @brief Parser for compact note-sequence strings.
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-09-12
 *
 * @note  Used by the SoundService RPCs (PlaySequence) so a host can audition
 *        sequences at runtime, e.g. "250:60 0:40 375:80".  The syntax is the
 *        same as the one the former tools/sound_synth_tester used:
 *
 *          freq:dur[:lfoHzx10[:lfoDepth]] [note2 ...]
 *
 *        freq 0 is a pause.  No libc / no float / no allocation — a hand-rolled
 *        parser keeps this tiny and stack-flat (Note[8] = 64 bytes).
 *
 *        @p len may be the exact text length, the length including the NUL, or
 *        the full size of a char[N] parameter padded with NULs (the generated Go
 *        client pads to sizeof(char[N]) and the generated C++ passes that payload
 *        size on). Only the text before the first NUL is parsed.
 */

#ifndef SOUND_SEQUENCE_HPP
#define SOUND_SEQUENCE_HPP

#include <cstddef>
#include <cstdint>

#include "sound_synth.hpp" /* Note, kMaxNotes */

namespace xbot::driver::sound {

/** @brief Longest sequence string "freq:dur:a:b " × kMaxNotes (for callers). */
constexpr size_t kMaxSequenceString = 160U;

/**
 * @brief Parse a note-sequence string into @p notes.
 *
 * Accepts the compact syntax above, separated by spaces.  Values are clamped to
 * uint16_t; a note with fewer than two fields is a parse error.
 *
 * @param text       Sequence string (no NUL terminator required).
 * @param len        Number of characters in @p text.
 * @param notes      Output array with room for @p max_notes entries.
 * @param max_notes  Capacity of @p notes (typically kMaxNotes).
 * @return Number of notes stored in @p notes, 0 on error or empty input.
 */
inline uint8_t parse_sequence(const char* text, size_t len, Note* notes, uint8_t max_notes) {
  uint8_t count = 0U;
  size_t i = 0U;
  while (i < len) {
    /* Skip separators (space, comma, semicolon, tab). */
    while (i < len && (text[i] == ' ' || text[i] == ',' || text[i] == ';' || text[i] == '\t')) {
      ++i;
    }
    /* End of input - or the end of the NUL-terminated text: a caller may pass the
       exact length, the length including the NUL, or a char[N] padded with NULs.
       Without this check the padding would be read as a note start and the whole
       sequence rejected (that is what `soundctl play sequence` used to hit). */
    if (i >= len || text[i] == '\0') break;
    if (count >= max_notes) break; /* silently drop extra notes */

    uint32_t v[4] = {0U, 0U, 0U, 0U};
    uint8_t fields = 0U;
    while (fields < 4U) {
      if (i >= len || text[i] < '0' || text[i] > '9') break; /* not a number */
      uint32_t value = 0U;
      while (i < len && text[i] >= '0' && text[i] <= '9') {
        value = value * 10U + static_cast<uint32_t>(text[i] - '0');
        if (value > 0xFFFFU) value = 0xFFFFU; /* clamp; avoids overflow */
        ++i;
      }
      v[fields++] = value;
      if (i < len && text[i] == ':') {
        ++i; /* more fields follow */
      } else {
        break;
      }
    }
    if (fields < 2U) return 0U; /* need at least freq:dur */

    notes[count].freq = static_cast<uint16_t>(v[0]);
    notes[count].duration_ms = static_cast<uint16_t>(v[1]);
    notes[count].lfo_hz_x10 = static_cast<uint16_t>(v[2]);
    notes[count].lfo_depth = static_cast<uint16_t>(v[3]);
    ++count;
  }
  return count;
}

}  // namespace xbot::driver::sound

#endif  // SOUND_SEQUENCE_HPP
