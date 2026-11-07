/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "sound_storage.hpp"

#include <ulog.h>

#include "hal.h"

namespace xbot::driver::sound {

namespace {

/* Persisted sound-definition overrides (LittleFS) — survive a reboot.
 *
 * Format: a header, then one record per overridden sound — a SoundId byte followed by
 * the raw SoundDefinition bytes (the layout is pinned by the static_assert in
 * sound_definition.hpp). Only the sounds the high level actually pushed are stored, so
 * the file stays small (16 B + n × 81 B instead of a full SoundId_count table) and no
 * validity bitmask is needed: the record's id says which sound it replaces.
 *
 * header_size and record_size are part of the header on purpose: any future layout change
 * is then *detected* and the file is ignored with a warning, instead of being read with
 * the wrong offsets and silently producing garbage definitions. */
constexpr const char* kSoundDefsPath = "/cfg/sound_defs.bin";
constexpr const char* kSoundDefsTmpPath = "/cfg/sound_defs.bin.tmp";
constexpr uint32_t kSoundDefsMagic = 0x53444631U;  // "SDF1"
constexpr uint16_t kSoundDefsVersion = 2U;

/** Bytes of one stored override: its SoundId plus the raw definition. */
constexpr size_t kOverrideRecordSize = sizeof(uint8_t) + sizeof(SoundDefinition);

/** On-disk layout of the persisted sound overrides. */
struct PersistedDefsHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t count;        ///< Number of override records that follow
  uint16_t header_size;  ///< sizeof(PersistedDefsHeader) — guards a header change
  uint16_t record_size;  ///< kOverrideRecordSize — guards a definition change
};
static_assert(sizeof(PersistedDefsHeader) == 12U, "PersistedDefsHeader layout changed — the store format is affected");

/** Runtime overrides, written by the SoundService and read by play_sound_id(). */
MUTEX_DECL(s_override_mutex);
SoundDefinition s_sound_overrides[SoundId_count];
bool s_override_valid[SoundId_count];

}  // namespace

void set_sound_override(SoundId id, const SoundDefinition& def) {
  const uint8_t idx = static_cast<uint8_t>(id);
  if (idx >= SoundId_count) return;

  chMtxLock(&s_override_mutex);
  s_sound_overrides[idx] = def;
  s_override_valid[idx] = true;
  chMtxUnlock(&s_override_mutex);
}

void clear_sound_overrides() {
  chMtxLock(&s_override_mutex);
  for (uint8_t i = 0U; i < SoundId_count; ++i) {
    s_override_valid[i] = false;
  }
  chMtxUnlock(&s_override_mutex);
}

bool get_sound_override(SoundId id, SoundDefinition& out) {
  const uint8_t idx = static_cast<uint8_t>(id);
  if (idx >= SoundId_count) return false;

  chMtxLock(&s_override_mutex);
  const bool valid = s_override_valid[idx];
  if (valid) {
    out = s_sound_overrides[idx];
  }
  chMtxUnlock(&s_override_mutex);
  return valid;
}

void load_sound_overrides_from_storage() {
  static File file;
  SoundDefinition def{};

  file.close();  // no-op unless an earlier attempt left it open
  if (file.open(kSoundDefsPath, LFS_O_RDONLY) != LFS_ERR_OK) {
    ULOG_INFO("Sound: no persisted overrides ('%s' missing), using ROM defaults", kSoundDefsPath);
    return;  // No persisted overrides — fall back to ROM defaults.
  }

  PersistedDefsHeader h{};
  const int n = file.read(&h, sizeof(h));
  if (n != static_cast<int>(sizeof(h)) || h.magic != kSoundDefsMagic || h.version != kSoundDefsVersion ||
      h.header_size != sizeof(PersistedDefsHeader) || h.record_size != kOverrideRecordSize || h.count > SoundId_count) {
    ULOG_WARNING("Sound: defs store invalid, ignoring");
    file.close();
    return;
  }

  // The store lists exactly the overridden sounds, so start from "none"
  clear_sound_overrides();

  uint16_t loaded = 0U;
  for (uint16_t i = 0U; i < h.count; ++i) {
    uint8_t id = 0U;
    const bool record_read = (file.read(&id, sizeof(id)) == static_cast<int>(sizeof(id))) &&
                             (file.read(&def, sizeof(def)) == static_cast<int>(sizeof(def)));
    if (!record_read || (id >= SoundId_count)) {
      ULOG_WARNING("Sound: defs store truncated, ignoring");
      clear_sound_overrides();  // do not keep a half applied store
      file.close();
      return;
    }
    set_sound_override(static_cast<SoundId>(id), def);
    ++loaded;
  }
  file.close();

  ULOG_INFO("Sound: loaded %hu override(s) from flash", loaded);
}

bool is_flash_error(int err) {
  return err == LFS_ERR_IO || err == LFS_ERR_CORRUPT;
}

bool write_staged(File& file, int& write_error, const void* data, size_t size) {
  const int written = file.write(const_cast<void*>(data), size);  // File::write() takes a non-const pointer
  if (written != static_cast<int>(size) && write_error == LFS_ERR_OK) {
    write_error = (written < 0) ? written : LFS_ERR_IO;
  }
  return written == static_cast<int>(size);
}

void save_sound_overrides_to_storage() {
  // Count first. The header carries it and every valid sound is one record.
  uint16_t count = 0U;
  chMtxLock(&s_override_mutex);
  for (uint8_t i = 0U; i < SoundId_count; ++i) {
    if (s_override_valid[i]) ++count;
  }
  chMtxUnlock(&s_override_mutex);

  // A File is ~330 bytes (256 B cache buffer) and this runs from the
  // SoundService thread's configuration callback, whose working area is only
  // 3 KB — the JSON parser frames are still live at this point. Keep a single
  // static instance (mkdirp() does not touch the file handle) off the stack.
  static File file;
  file.close();  // no-op unless an earlier attempt left it open

  // Ensure the /cfg directory exists (idempotent).
  if (file.mkdirp(kSoundDefsPath) != LFS_ERR_OK) {
    ULOG_WARNING("Sound: cannot create defs store dir");
    return;
  }

  // Stage the store in a temp file and rename it over kSoundDefsPath only once it is complete
  if (file.open(kSoundDefsTmpPath, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC) != LFS_ERR_OK) {
    ULOG_WARNING("Sound: cannot open defs store '%s'", kSoundDefsTmpPath);
    return;
  }

  PersistedDefsHeader h{};
  h.magic = kSoundDefsMagic;
  h.version = kSoundDefsVersion;
  h.count = count;
  h.header_size = sizeof(PersistedDefsHeader);
  h.record_size = kOverrideRecordSize;

  // The first failing write decides whether the staged file may be dropped again below.
  int write_error = LFS_ERR_OK;
  bool ok = write_staged(file, write_error, &h, sizeof(h));
  for (uint8_t i = 0U; ok && i < SoundId_count; ++i) {
    SoundDefinition def{};
    chMtxLock(&s_override_mutex);
    const bool valid = s_override_valid[i];
    if (valid) {
      def = s_sound_overrides[i];
    }
    chMtxUnlock(&s_override_mutex);
    if (!valid) continue;

    const uint8_t id = i;
    ok = write_staged(file, write_error, &id, sizeof(id)) && write_staged(file, write_error, &def, sizeof(def));
  }

  // sync() flushes the staged file and commits its size
  const int sync_result = file.sync();
  file.close();

  // A flash failure leaves LittleFS's program cache dirty (lfs_bd_flush() returns without
  // clearing it), and programming another block through that cache hits an LFS_ASSERT — so
  // only touch the filesystem again while the flash is known to be healthy. sync() can still
  // report success in that state, which is why both errors are checked. A staged file that
  // is left behind is harmless: nothing reads it and the next save truncates it again.
  const bool flash_healthy = !is_flash_error(write_error) && !is_flash_error(sync_result);

  if (!ok || sync_result != LFS_ERR_OK) {
    ULOG_WARNING("Sound: defs store write failed (write=%d, sync=%d), previous store kept", write_error, sync_result);
    if (flash_healthy) lfs_remove(&lfs, kSoundDefsTmpPath);  // best effort: drop the partial file
    return;
  }

  const int rename_result = lfs_rename(&lfs, kSoundDefsTmpPath, kSoundDefsPath);
  if (rename_result != LFS_ERR_OK) {
    ULOG_WARNING("Sound: cannot replace defs store (%d), previous store kept", rename_result);
    if (flash_healthy && !is_flash_error(rename_result)) lfs_remove(&lfs, kSoundDefsTmpPath);  // best effort
    return;
  }

  const unsigned int file_bytes = static_cast<unsigned int>(sizeof(h) + (count * kOverrideRecordSize));
  ULOG_INFO("Sound: persisted %hu override(s) (%u bytes) to %s", count, file_bytes, kSoundDefsPath);
}

}  // namespace xbot::driver::sound
