/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sound_storage.hpp
 * @brief Runtime sound overrides and the shared LittleFS store helpers.
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-11-07
 *
 * @note  The high level pushes a definitions blob; the parsed definitions are kept in
 *        RAM (one SoundDefinition + a validity byte per SoundId) and persisted to
 *        /cfg/sound_defs.bin, so a reboot starts with the last-known sounds instead of the
 *        ROM defaults. The store is staged in a .tmp file and renamed on commit, so a
 *        rejected blob or a power loss leaves the previous one in place.
 */

#ifndef SOUND_STORAGE_HPP
#define SOUND_STORAGE_HPP

#include <cstddef>

#include "filesystem/file.hpp"
#include "sound_definition.hpp"

namespace xbot::driver::sound {

/**
 * @brief Register (or replace) a runtime override for one logical sound.
 *
 * Overrides are populated by the SoundService from the high-level configuration blob and
 * take precedence over the ROM defaults.
 */
void set_sound_override(SoundId id, const SoundDefinition& def);

/** @brief Remove all runtime sound overrides, restoring the ROM defaults. */
void clear_sound_overrides();

/** @brief Load the override for @p id; false when the sound has no override. */
bool get_sound_override(SoundId id, SoundDefinition& out);

/**
 * @brief Load persisted sound overrides from LittleFS (if any).
 *
 * Called at boot (player_init) so early sounds use the last-known high-level definitions
 * instead of the ROM defaults.
 */
void load_sound_overrides_from_storage();

/**
 * @brief Persist the current sound overrides to LittleFS.
 *
 * Called by the SoundService after a successful definitions-blob parse, so the overrides
 * survive a reboot.
 */
void save_sound_overrides_to_storage();

/** @brief A flash failure, as opposed to a request the filesystem rejected (e.g. a full one). */
bool is_flash_error(int err);

/**
 * @brief Write one field of a staged store and remember its first failing write.
 *
 * @param file        The open staged store.
 * @param write_error First failing error code (kept; LFS_ERR_OK while everything works).
 * @return true when all bytes were written.
 */
bool write_staged(File& file, int& write_error, const void* data, size_t size);

}  // namespace xbot::driver::sound

#endif  // SOUND_STORAGE_HPP
