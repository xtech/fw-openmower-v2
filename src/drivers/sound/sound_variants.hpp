/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sound_variants.hpp
 * @brief Several takes per sound: the persisted list and the pick policy.
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-11-07
 *
 * @note  The high level may send several MP3s for one event (a spoken announcement with
 *        different wording, or a sound with several recordings); the low level plays one
 *        of them. The lists are persisted per sound (the file format lives in
 *        sound_variants.cpp) and read back only when the sound plays: RAM keeps a 4-byte
 *        reference per sound and a one-byte rotate state, the names (a few hundred bytes)
 *        come from LittleFS - which is where the MP3 is streamed from anyway, so the read
 *        costs nothing next to the audio.
 *
 *        The SoundService writes the lists while it parses the definitions blob; nothing
 *        is replaced before commit_sound_variants_write(), so a rejected blob leaves the
 *        previous lists in place.
 */

#ifndef SOUND_VARIANTS_HPP
#define SOUND_VARIANTS_HPP

#include <etl/string_view.h>

#include <cstddef>
#include <cstdint>

#include "sound_definition.hpp"  // kMaxPath

namespace xbot::driver::sound {

/** @brief Maximum number of takes stored for one sound. */
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
 * @brief Choose one take for @p id and write the complete "/sounds/<name>" path.
 *
 * @param path      Destination buffer (typically SoundDefinition::path).
 * @param path_size Capacity of @p path (kMaxPath).
 * @return false when the sound has no usable take list; the caller keeps the path of the
 *         definition (which is take 1, the fallback).
 */
bool choose_variant(SoundId id, char* path, size_t path_size);

}  // namespace xbot::driver::sound

#endif  // SOUND_VARIANTS_HPP
