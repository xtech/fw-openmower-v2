/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file file_service.cpp
 * @brief Generic LittleFS file-transfer service (chunked upload + content hash).
 *
 * @note  Versioned file storage for the high-level system:
 *        - FileWrite: appends a chunk to `<path>.tmp`; a non-zero Hash marks the
 *          final chunk and atomically commits the upload (rename + sidecar).
 *        - FileExists(path, hash): true iff the file exists (and, when hash != 0,
 *          its sidecar matches).
 *        - FileRemove: deletes a file and its sidecar.
 */

#include "file_service.hpp"

#include <ulog.h>

#include <cstring>
#include <xbot-service/Io.hpp>

#include "filesystem/file.hpp"

namespace {

/** Shortcut for the path budget the RPC layer accepts (see file_service.json). */
constexpr size_t kMaxPath = FileService::kMaxPath;

/** Copies an RPC path parameter (a len delimited blob) into a null-terminated buffer. */
void copy_path(const void* src, uint32_t src_len, char* dst, size_t dst_size) {
  size_t n = (src_len < dst_size - 1U) ? src_len : (dst_size - 1U);
  memcpy(dst, src, n);
  dst[n] = '\0';
}

bool file_exists(const char* path) {
  struct lfs_info info;
  return lfs_stat(&lfs, path, &info) == LFS_ERR_OK;
}

bool read_sidecar_hash(const char* path, uint32_t& out_hash) {
  char hash_path[kMaxPath + 6];
  strcpy(hash_path, path);
  strcat(hash_path, ".hash");

  File file;
  if (file.open(hash_path, LFS_O_RDONLY) != LFS_ERR_OK) {
    return false;
  }
  uint32_t hash = 0;
  int n = file.read(&hash, sizeof(hash));
  file.close();
  if (n != static_cast<int>(sizeof(hash))) {
    return false;
  }
  out_hash = hash;
  return true;
}

/**
 * @brief Commit a fully-written `<path>.tmp` upload to `<path>`.
 *
 * Order matters for crash-safety: the old sidecar is removed first so a stale
 * hash can never point at a partially-updated file, then the upload is renamed
 * over the destination — lfs_rename() replaces an existing same-type destination
 * (here always a file) within a single commit, so the old version stays intact
 * until the new one is in place — and finally the sidecar is written.
 */
int finalize_file(const char* path, uint32_t hash) {
  char hash_path[kMaxPath + 6];
  strcpy(hash_path, path);
  strcat(hash_path, ".hash");

  lfs_remove(&lfs, hash_path);  // best effort: invalidate previous version

  char tmp[kMaxPath + 5];
  strcpy(tmp, path);
  strcat(tmp, ".tmp");
  int result = lfs_rename(&lfs, tmp, path);
  if (result != LFS_ERR_OK) {
    return result;
  }

  File file;
  result = file.open(hash_path, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
  if (result != LFS_ERR_OK) {
    return result;
  }
  int written = file.write(&hash, sizeof(hash));
  file.sync();
  file.close();
  if (written != static_cast<int>(sizeof(hash))) {
    return LFS_ERR_IO;
  }
  return LFS_ERR_OK;
}

}  // namespace

void FileService::RPCFileExists(uint16_t call_id, const uint8_t* Path, uint32_t PathLen, uint32_t Hash) {
  char path[kMaxPath + 1];
  copy_path(Path, PathLen, path, sizeof(path));

  uint8_t result = 0;
  if (file_exists(path)) {
    if (Hash == 0U) {
      result = 1;
    } else {
      uint32_t stored = 0;
      if (read_sidecar_hash(path, stored) && stored == Hash) {
        result = 1;
      }
    }
  }

  ULOG_INFO("File: exists '%s' -> %hhu (hash=%#010x)", path, result, Hash);

  SendRpcResponse(call_id, xbot::datatypes::RpcStatus::SUCCESS, &result, sizeof(result));
}

void FileService::RPCFileRemove(uint16_t call_id, const uint8_t* Path, uint32_t PathLen) {
  char path[kMaxPath + 1];
  copy_path(Path, PathLen, path, sizeof(path));

  uint8_t result = 0;
  int remove_result = lfs_remove(&lfs, path);
  if (remove_result != LFS_ERR_OK && remove_result != LFS_ERR_NOENT) {
    ULOG_WARNING("File: remove '%s' failed (%d)", path, remove_result);
    result = 1;
  } else {
    ULOG_INFO("File: removed '%s'", path);
    char hash_path[kMaxPath + 6];
    strcpy(hash_path, path);
    strcat(hash_path, ".hash");
    lfs_remove(&lfs, hash_path);  // best effort
  }

  SendRpcResponse(call_id, xbot::datatypes::RpcStatus::SUCCESS, &result, sizeof(result));
}

void FileService::abort_upload() {
  if (upload_.isOpen()) {
    upload_.close();
  }
  upload_written_ = 0;
  upload_path_[0] = '\0';
}

void FileService::OnStop() {
  // The owner that opened the upload is gone (claim, or the service is stopped for a
  // reconfiguration); a stale handle would only keep the cache occupied.
  abort_upload();
}

void FileService::RPCFileWrite(uint16_t call_id, const uint8_t* Path, uint32_t PathLen, uint32_t Offset,
                               const uint8_t* Data, uint32_t DataLen, uint32_t Hash) {
  char path[kMaxPath + 1];
  copy_path(Path, PathLen, path, sizeof(path));

  if (Offset == 0U) {
    ULOG_INFO("File: write '%s' (start)", path);
  }

  int32_t result = 0;

  do {
    char tmp[kMaxPath + 5];
    strcpy(tmp, path);
    strcat(tmp, ".tmp");

    // The common case is a chunk that continues the open session: no open, no seek and
    // no sync. Anything else - the first chunk, another file, or a session that a
    // restart dropped - reopens the staging file at the requested offset, so an
    // interrupted upload can be continued where it stopped.
    const bool continues_upload = upload_.isOpen() && strcmp(upload_path_, path) == 0 && Offset == upload_written_;
    if (!continues_upload) {
      abort_upload();

      // Create parent directories once, on the first chunk only (skip root-level
      // paths, which need none). Doing this on every chunk would re-traverse the
      // directory tree each time.
      if (Offset == 0U && strchr(path + 1, '/') != nullptr) {
        int r = upload_.mkdirp(path);
        if (r != LFS_ERR_OK) {
          ULOG_WARNING("File: mkdirp '%s' failed (%d)", path, r);
          result = r;
          break;
        }
      }

      const int flags = (Offset == 0U) ? (LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC)
                                       : (LFS_O_WRONLY | LFS_O_CREAT);  // keep what was staged
      int r = upload_.open(tmp, flags);
      if (r != LFS_ERR_OK) {
        ULOG_WARNING("File: open '%s' failed (%d)", tmp, r);
        result = r;
        break;
      }

      if (Offset > 0U) {
        // Continuing after a restart: the staged bytes have to be there, otherwise the
        // write would leave a hole that the hash of the last chunk would still vouch for.
        const int staged = upload_.size();
        if (staged < static_cast<int>(Offset)) {
          ULOG_WARNING("File: '%s' has %d of %lu staged bytes, dropping it", tmp, staged,
                       static_cast<unsigned long>(Offset));
          abort_upload();
          lfs_remove(&lfs, tmp);
          result = LFS_ERR_INVAL;
          break;
        }
        r = upload_.seek(static_cast<lfs_soff_t>(Offset), LFS_SEEK_SET);
        if (r < 0) {
          ULOG_WARNING("File: seek '%s' failed (%d)", tmp, r);
          abort_upload();
          result = r;
          break;
        }
      }

      strncpy(upload_path_, path, kMaxPath);
      upload_path_[kMaxPath] = '\0';
      upload_written_ = Offset;
    }

    const int written = upload_.write(const_cast<uint8_t*>(Data), DataLen);
    if (written != static_cast<int>(DataLen)) {
      // LittleFS returns the full count or a negative error, but a short write must never be
      // committed: the hash below would then vouch for a truncated file.
      ULOG_WARNING("File: write '%s' failed (%d of %u bytes)", tmp, written, static_cast<unsigned int>(DataLen));
      abort_upload();
      lfs_remove(&lfs, tmp);  // do not let the next chunk continue a damaged staging file
      result = (written < 0) ? written : LFS_ERR_IO;
      break;
    }
    upload_written_ += DataLen;
    result = written;

    if (Hash != 0U) {
      // Only the last chunk pays for the flush: the whole upload was staged in <path>.tmp
      // and is committed atomically below, so intermediate chunks need no durability.
      const int sync_result = upload_.sync();
      abort_upload();
      if (sync_result != LFS_ERR_OK) {
        ULOG_WARNING("File: sync '%s' failed (%d)", tmp, sync_result);
        result = sync_result;
        break;
      }
      const int finalize_result = finalize_file(path, Hash);
      if (finalize_result != LFS_ERR_OK) {
        ULOG_WARNING("File: finalize '%s' failed (%d)", path, finalize_result);
        result = finalize_result;
      } else {
        ULOG_INFO("File: committed '%s' (hash=%#010x)", path, Hash);
      }
    }
  } while (false);

  SendRpcResponse(call_id, xbot::datatypes::RpcStatus::SUCCESS, &result, sizeof(result));
}

void FileService::RPCFileList(uint16_t call_id, const uint8_t* Path, uint32_t PathLen, uint32_t StartIndex,
                              uint8_t* data, uint16_t* response_length) {
  char path[kMaxPath + 1];
  copy_path(Path, PathLen, path, sizeof(path));

  // Strip trailing '/' so entries can be joined with a single separator.
  size_t path_len = strlen(path);
  while (path_len > 1U && path[path_len - 1U] == '/') {
    path[--path_len] = '\0';
  }

  // Wire format (see FileList in file_service.json):
  //   [0..4)  uint32 total_count
  //   [4..8)  uint32 entry_count   (as many as fit into the return value)
  //   [8..)   entry_count × (char[128] full_path + uint32 hash)
  constexpr uint16_t kEntrySize = 128U + static_cast<uint16_t>(sizeof(uint32_t));
  constexpr uint16_t kHeaderSize = 2U * static_cast<uint16_t>(sizeof(uint32_t));

  // The dispatcher hands in the capacity of the return value, so a caller with a larger
  // buffer gets more entries per call instead of exactly one.
  const uint16_t capacity =
      (*response_length > kHeaderSize) ? static_cast<uint16_t>((*response_length - kHeaderSize) / kEntrySize) : 0U;

  uint32_t total_count = 0;
  uint32_t entry_count = 0;

  lfs_dir_t dir;
  if (lfs_dir_open(&lfs, &dir, path) == LFS_ERR_OK) {
    struct lfs_info info;
    uint32_t idx = 0;
    while (lfs_dir_read(&lfs, &dir, &info) > 0) {
      if (info.type != LFS_TYPE_REG) continue;

      const size_t name_len = strlen(info.name);
      if (name_len == 0U || info.name[0] == '.') continue;
      if (name_len >= 5U && strcmp(info.name + name_len - 5U, ".hash") == 0) continue;
      if (name_len >= 4U && strcmp(info.name + name_len - 4U, ".tmp") == 0) continue;

      if (idx >= StartIndex && entry_count < capacity) {
        char full_path[kMaxPath + 1];
        size_t full_len = path_len;
        if (full_len >= sizeof(full_path)) full_len = sizeof(full_path) - 1U;
        memcpy(full_path, path, full_len);
        if (full_len > 0U && full_path[full_len - 1U] != '/' && full_len < sizeof(full_path) - 1U) {
          full_path[full_len++] = '/';
        }
        const size_t remaining = sizeof(full_path) - full_len;
        if (name_len < remaining) {
          memcpy(full_path + full_len, info.name, name_len + 1U);

          uint8_t* entry = data + kHeaderSize + entry_count * kEntrySize;
          memset(entry, 0, 128U);
          strncpy(reinterpret_cast<char*>(entry), full_path, 127U);
          uint32_t hash = 0U;
          read_sidecar_hash(full_path, hash);
          memcpy(entry + 128U, &hash, sizeof(hash));
          entry_count++;
        }
      }
      idx++;
    }
    lfs_dir_close(&lfs, &dir);
    total_count = idx;
  }

  if (StartIndex == 0U) {
    ULOG_INFO("File: list '%s' (%u files)", path, total_count);
  }

  memcpy(data, &total_count, sizeof(total_count));
  memcpy(data + 4U, &entry_count, sizeof(entry_count));
  *response_length = static_cast<uint16_t>(kHeaderSize + entry_count * kEntrySize);

  SendRpcResponse(call_id, xbot::datatypes::RpcStatus::SUCCESS, data, *response_length);
}

/**
 * @brief Report the LittleFS usage.
 *
 * See FileStats in file_service.json: four uint32 - total, used, free, block size.
 * free keeps one block back, because LittleFS needs a free block while it updates
 * metadata. Array returns carry the element count in response_length (see the RPC
 * contract in the generated base class), not the byte count.
 */
void FileService::RPCFileStats(uint16_t call_id, uint32_t* data, uint16_t* response_length) {
  const uint32_t block_size = static_cast<uint32_t>(lfs.cfg->block_size);
  const uint32_t block_count = static_cast<uint32_t>(lfs.cfg->block_count);
  const lfs_ssize_t used = lfs_fs_size(&lfs);

  /* lfs_fs_size() counts allocated blocks; a negative result means the walk failed,
     so report the storage as full to stay on the safe side. */
  uint32_t used_blocks = (used < 0) ? block_count : static_cast<uint32_t>(used);
  if (used_blocks > block_count) used_blocks = block_count;

  const uint32_t total_bytes = block_count * block_size;
  const uint32_t used_bytes = used_blocks * block_size;
  const uint32_t free_bytes = (used_blocks + 1U < block_count) ? ((block_count - used_blocks - 1U) * block_size) : 0U;

  ULOG_INFO("File: stats %lu of %lu bytes used, %lu free (block %lu B)", static_cast<unsigned long>(used_bytes),
            static_cast<unsigned long>(total_bytes), static_cast<unsigned long>(free_bytes),
            static_cast<unsigned long>(block_size));

  data[0] = total_bytes;
  data[1] = used_bytes;
  data[2] = free_bytes;
  data[3] = block_size;
  *response_length = 4U;

  SendRpcResponse(call_id, xbot::datatypes::RpcStatus::SUCCESS, data, *response_length * sizeof(uint32_t));
}
