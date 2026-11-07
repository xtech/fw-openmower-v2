/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "sound_variants.hpp"

#include <ulog.h>

#include <cstring>

#include "filesystem/file.hpp"
#include "hal.h"
#include "sound_random.hpp"
#include "sound_storage.hpp"

namespace xbot::driver::sound {

namespace {

/*
 * Layout of /cfg/sound_variants.bin:
 *
 *   header (12 B: magic, version, index_size, name_size)
 *   index  (SoundId_count x PersistedVariantRef, fixed position per sound)
 *   data   (per take one NUL padded kVariantNameMax name; the "/sounds/" prefix
 *           is a low level contract and is not stored)
 *
 * The index sits at a fixed position, so an entry is one seek away and no search
 * is needed. Written like the definition store: staged in a .tmp file that is
 * renamed on commit, so a rejected blob or a power loss leaves the previous lists
 * in place. A header whose sizes do not match the compiled layout is ignored,
 * which simply means every sound plays its first take.
 */

constexpr const char* kVariantsPath = "/cfg/sound_variants.bin";
constexpr const char* kVariantsTmpPath = "/cfg/sound_variants.bin.tmp";
constexpr uint32_t kVariantsMagic = 0x53445631U;  // "SDV1"
constexpr uint16_t kVariantsVersion = 1U;

struct PersistedVariantRef {
  uint16_t offset;  ///< byte offset of take 1 in the store (0 = no takes)
  uint8_t count;    ///< number of takes
  uint8_t pick;     ///< PickMode
};
static_assert(sizeof(PersistedVariantRef) == 4U, "PersistedVariantRef layout changed - the store format is affected");

struct PersistedVariantsHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t index_size;  ///< sizeof(PersistedVariantRef) - guards an index change
  uint16_t name_size;   ///< kVariantNameMax - guards a name-length change
  uint16_t reserved;
};
static_assert(sizeof(PersistedVariantsHeader) == 12U,
              "PersistedVariantsHeader layout changed - the store format is affected");

/** Bytes of the store before the names start (header + one ref per SoundId). */
constexpr uint16_t kVariantsDataOffset =
    static_cast<uint16_t>(sizeof(PersistedVariantsHeader) + (SoundId_count * sizeof(PersistedVariantRef)));

PersistedVariantRef s_variant_ref[SoundId_count];  ///< index of the persisted store
uint8_t s_variant_last[SoundId_count];             ///< ROTATE: the take that played last

/* Write state of a staged store (begin_sound_variants_write .. commit/abort). */
File s_variant_out;              ///< the staged store (writer side)
File s_variant_in;               ///< the store readers share this one
MUTEX_DECL(s_variant_io_mutex);  ///< serialises the readers
MUTEX_DECL(s_variant_mutex);     ///< guards the index and the ROTATE state
bool s_variant_writing = false;
uint16_t s_variant_next_offset = kVariantsDataOffset;
PersistedVariantRef s_variant_pending[SoundId_count];
bool s_variant_set_open = false;
uint8_t s_variant_set_id = 0U;
uint16_t s_variant_set_offset = 0U;
uint8_t s_variant_set_count = 0U;

/** True for a take name that ends in ".mp3" (case-insensitive). */
bool has_mp3_suffix(etl::string_view name) {
  if (name.size() <= 4U) return false;
  const etl::string_view ext = name.substr(name.size() - 4U);
  return ext[0] == '.' && (ext[1] == 'm' || ext[1] == 'M') && (ext[2] == 'p' || ext[2] == 'P') && ext[3] == '3';
}

/** Reads the names of one take set. Returns false when the store cannot deliver. */
bool read_variant_names(const PersistedVariantRef& ref, char names[kMaxVariants][kVariantNameMax]) {
  chMtxLock(&s_variant_io_mutex);
  s_variant_in.close(); /* no-op unless an earlier attempt left it open */
  bool ok = s_variant_in.open(kVariantsPath, LFS_O_RDONLY) == LFS_ERR_OK;
  if (ok) ok = s_variant_in.seek(static_cast<lfs_soff_t>(ref.offset), LFS_SEEK_SET) >= 0;
  for (uint8_t i = 0U; ok && i < ref.count; ++i) {
    ok = s_variant_in.read(names[i], kVariantNameMax) == static_cast<int>(kVariantNameMax);
    names[i][kVariantNameMax - 1U] = '\0';
    if (ok) {
      /* A corrupt store - or a read that raced a commit between its rename and the
         index swap - must not end in playing a garbage path: fall back to take 1. */
      ok = has_mp3_suffix(etl::string_view(names[i]));
    }
  }
  s_variant_in.close();
  chMtxUnlock(&s_variant_io_mutex);
  return ok;
}

}  // namespace

bool choose_variant(SoundId id, char* path, size_t path_size) {
  const uint8_t idx = static_cast<uint8_t>(id);
  if (idx >= SoundId_count || path == nullptr || path_size == 0U) return false;

  PersistedVariantRef ref{};
  chMtxLock(&s_variant_mutex);
  ref = s_variant_ref[idx];
  chMtxUnlock(&s_variant_mutex);

  const PickMode pick = static_cast<PickMode>(ref.pick);
  if (ref.count <= 1U || ref.count > kMaxVariants || pick == PickMode::FIRST) {
    return false; /* one take (or "first") needs no flash read at all */
  }

  /* Largest possible list: kMaxVariants x kVariantNameMax = 448 B on the caller's
     stack, transient. */
  char names[kMaxVariants][kVariantNameMax];
  if (!read_variant_names(ref, names)) {
    ULOG_WARNING("Sound: take store unreadable, '%s' plays its first take", SoundId_to_string(id));
    return false;
  }

  /* Take and ROTATE state under the lock: several threads trigger sounds. */
  uint8_t take = 0U;
  chMtxLock(&s_variant_mutex);
  if (pick == PickMode::ROTATE) {
    take = static_cast<uint8_t>((s_variant_last[idx] + 1U) % ref.count);
  } else {
    take = static_cast<uint8_t>(random_below(ref.count));
  }
  s_variant_last[idx] = take;
  chMtxUnlock(&s_variant_mutex);

  constexpr size_t kPrefixLen = sizeof("/sounds/") - 1U;
  const size_t name_len = strlen(names[take]);
  if (kPrefixLen + name_len + 1U > path_size) {
    ULOG_WARNING("Sound: take name too long for the path buffer");
    return false;
  }
  memcpy(path, "/sounds/", kPrefixLen);
  memcpy(path + kPrefixLen, names[take], name_len + 1U);
  return true;
}

static PersistedVariantsHeader variant_header() {
  PersistedVariantsHeader h{};
  h.magic = kVariantsMagic;
  h.version = kVariantsVersion;
  h.index_size = sizeof(PersistedVariantRef);
  h.name_size = static_cast<uint16_t>(kVariantNameMax);
  return h;
}

void begin_sound_variants_write() {
  if (s_variant_writing) abort_sound_variants_write();
  for (uint8_t i = 0U; i < SoundId_count; ++i) {
    s_variant_pending[i] = PersistedVariantRef{};
  }
  s_variant_set_open = false;
  s_variant_set_count = 0U;
  s_variant_next_offset = kVariantsDataOffset;

  /* mkdirp() lives on File; borrow the reader instance for that one call. */
  chMtxLock(&s_variant_io_mutex);
  const int mkdir_result = s_variant_in.mkdirp(kVariantsPath);
  chMtxUnlock(&s_variant_io_mutex);
  if (mkdir_result != LFS_ERR_OK) {
    ULOG_WARNING("Sound: cannot create the take store dir");
    return;
  }
  if (s_variant_out.open(kVariantsTmpPath, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC) != LFS_ERR_OK) {
    ULOG_WARNING("Sound: cannot open the take store '%s'", kVariantsTmpPath);
    return;
  }

  /* Reserve header and index; the commit writes both again. */
  int err = LFS_ERR_OK;
  const PersistedVariantsHeader h = variant_header();
  bool ok = write_staged(s_variant_out, err, &h, sizeof(h));
  for (uint8_t i = 0U; ok && i < SoundId_count; ++i) {
    const PersistedVariantRef zero{};
    ok = write_staged(s_variant_out, err, &zero, sizeof(zero));
  }
  if (!ok) {
    ULOG_WARNING("Sound: take store write failed (%d)", err);
    s_variant_out.close();
    lfs_remove(&lfs, kVariantsTmpPath); /* best effort */
    return;
  }
  s_variant_writing = true;
}

void begin_sound_variant_set(SoundId id) {
  const uint8_t idx = static_cast<uint8_t>(id);
  if (idx >= SoundId_count) return;
  s_variant_set_open = true;
  s_variant_set_id = idx;
  s_variant_set_offset = s_variant_next_offset;
  s_variant_set_count = 0U;
}

bool add_sound_variant(etl::string_view name) {
  /* A blob that carries takes must be stored: accepting it while the writer is inactive
     would leave the previous take lists in place and the player would mix them with the
     new definitions. Without takes the definitions still apply (the caller never reaches
     this function), so a read-only flash only rejects variant definitions. */
  if (!s_variant_writing) {
    ULOG_ERROR("Sound config: take store not writable, rejecting the blob");
    return false;
  }
  if (!s_variant_set_open) {
    ULOG_ERROR("Sound config: take outside a 'variants' array");
    return false;
  }
  if (name.size() + 1U > kVariantNameMax) {
    ULOG_ERROR("Sound config: take name too long (%.*s)", static_cast<int>(name.size()), name.data());
    return false;
  }
  /* The player concatenates the name with "/sounds/", so an empty name or one
     that is not an MP3 has to fail here instead of at play time. */
  if (!has_mp3_suffix(name)) {
    ULOG_ERROR("Sound config: take '%.*s' is not an .mp3 file name", static_cast<int>(name.size()), name.data());
    return false;
  }
  if (s_variant_set_count >= kMaxVariants) {
    ULOG_ERROR("Sound config: more than %hhu takes for one sound", kMaxVariants);
    return false;
  }

  /* Fixed stride, NUL padded - the reader indexes it without a length table. */
  char record[kVariantNameMax]{};
  for (size_t i = 0U; i < name.size(); ++i) {
    record[i] = name[i];
  }

  int err = LFS_ERR_OK;
  if (!write_staged(s_variant_out, err, record, sizeof(record))) {
    ULOG_WARNING("Sound: take store write failed (%d)", err);
    return false;
  }
  s_variant_set_count++;
  s_variant_next_offset = static_cast<uint16_t>(s_variant_next_offset + sizeof(record));
  return true;
}

void end_sound_variant_set() {
  if (s_variant_set_open && s_variant_set_count > 0U) {
    s_variant_pending[s_variant_set_id].offset = s_variant_set_offset;
    s_variant_pending[s_variant_set_id].count = s_variant_set_count;
  }
  s_variant_set_open = false;
  s_variant_set_count = 0U;
}

void set_sound_variant_pick(SoundId id, PickMode pick) {
  const uint8_t idx = static_cast<uint8_t>(id);
  if (idx >= SoundId_count) return;
  s_variant_pending[idx].pick = static_cast<uint8_t>(pick);
}

void commit_sound_variants_write() {
  if (!s_variant_writing) return;
  s_variant_writing = false;

  int err = LFS_ERR_OK;
  bool ok = s_variant_out.seek(0, LFS_SEEK_SET) >= 0;
  const PersistedVariantsHeader h = variant_header();
  ok = ok && write_staged(s_variant_out, err, &h, sizeof(h));
  for (uint8_t i = 0U; ok && i < SoundId_count; ++i) {
    ok = write_staged(s_variant_out, err, &s_variant_pending[i], sizeof(PersistedVariantRef));
  }
  const int sync_result = s_variant_out.sync();
  s_variant_out.close();

  const bool flash_healthy = !is_flash_error(err) && !is_flash_error(sync_result);
  if (!ok || sync_result != LFS_ERR_OK) {
    ULOG_WARNING("Sound: take store write failed (write=%d, sync=%d), previous store kept", err, sync_result);
    if (flash_healthy) lfs_remove(&lfs, kVariantsTmpPath); /* best effort */
    return;
  }
  const int rename_result = lfs_rename(&lfs, kVariantsTmpPath, kVariantsPath);
  if (rename_result != LFS_ERR_OK) {
    ULOG_WARNING("Sound: cannot replace the take store (%d), previous store kept", rename_result);
    if (flash_healthy && !is_flash_error(rename_result)) lfs_remove(&lfs, kVariantsTmpPath);
    return;
  }
  load_sound_variants_from_storage();
}

void abort_sound_variants_write() {
  if (!s_variant_writing) return;
  s_variant_writing = false;
  s_variant_set_open = false;
  s_variant_out.close();
  lfs_remove(&lfs, kVariantsTmpPath); /* best effort */
}

void load_sound_variants_from_storage() {
  PersistedVariantRef refs[SoundId_count]{};
  PersistedVariantsHeader h{};

  chMtxLock(&s_variant_io_mutex);
  s_variant_in.close(); /* no-op unless an earlier attempt left it open */
  bool ok = s_variant_in.open(kVariantsPath, LFS_O_RDONLY) == LFS_ERR_OK;
  if (ok) ok = s_variant_in.read(&h, sizeof(h)) == static_cast<int>(sizeof(h));
  if (ok) {
    ok = h.magic == kVariantsMagic && h.version == kVariantsVersion && h.index_size == sizeof(PersistedVariantRef) &&
         h.name_size == kVariantNameMax;
  }
  if (ok) ok = s_variant_in.read(refs, sizeof(refs)) == static_cast<int>(sizeof(refs));
  const int file_size = ok ? s_variant_in.size() : 0;
  s_variant_in.close();
  chMtxUnlock(&s_variant_io_mutex);

  chMtxLock(&s_variant_mutex);
  uint8_t with_takes = 0U;
  for (uint8_t i = 0U; i < SoundId_count; ++i) {
    PersistedVariantRef r = ok ? refs[i] : PersistedVariantRef{};
    /* A truncated or foreign file must not make the player read past its end. */
    const uint32_t end = static_cast<uint32_t>(r.offset) + (static_cast<uint32_t>(r.count) * kVariantNameMax);
    if (r.count == 0U || r.count > kMaxVariants || end > static_cast<uint32_t>(file_size)) {
      r = PersistedVariantRef{};
    }
    if (r.count > 0U) with_takes++;
    s_variant_ref[i] = r;
  }
  chMtxUnlock(&s_variant_mutex);

  if (ok) {
    ULOG_INFO("Sound: %hhu take list(s) restored from %s", with_takes, kVariantsPath);
  } else {
    ULOG_INFO("Sound: no usable take store ('%s'), every sound plays its first take", kVariantsPath);
  }
}

}  // namespace xbot::driver::sound
