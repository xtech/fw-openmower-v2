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

/** Maximum path length accepted by the RPC layer (see file_service.json). */
constexpr size_t kMaxPath = 128U;

/** Copies an RPC string parameter into a guaranteed null-terminated buffer. */
void copy_path(const char* src, uint32_t src_len, char* dst, size_t dst_size) {
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
 * hash can never point at a partially-updated file, then the old file is
 * removed, the upload renamed into place, and finally the sidecar written.
 */
int finalize_file(const char* path, uint32_t hash) {
  char hash_path[kMaxPath + 6];
  strcpy(hash_path, path);
  strcat(hash_path, ".hash");

  lfs_remove(&lfs, hash_path);  // best effort: invalidate previous version

  int result = lfs_remove(&lfs, path);
  if (result != LFS_ERR_OK && result != LFS_ERR_NOENT) {
    return result;
  }

  char tmp[kMaxPath + 5];
  strcpy(tmp, path);
  strcat(tmp, ".tmp");
  result = lfs_rename(&lfs, tmp, path);
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

void FileService::RPCFileExists(uint16_t call_id, const char* Path, uint32_t PathLen, uint32_t Hash) {
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

  SendRpcResponse(call_id, xbot::datatypes::RpcStatus::SUCCESS, &result, sizeof(result));
}

void FileService::RPCFileRemove(uint16_t call_id, const char* Path, uint32_t PathLen) {
  char path[kMaxPath + 1];
  copy_path(Path, PathLen, path, sizeof(path));

  uint8_t result = 0;
  int remove_result = lfs_remove(&lfs, path);
  if (remove_result != LFS_ERR_OK && remove_result != LFS_ERR_NOENT) {
    ULOG_WARNING("File: remove '%s' failed (%d)", path, remove_result);
    result = 1;
  } else {
    char hash_path[kMaxPath + 6];
    strcpy(hash_path, path);
    strcat(hash_path, ".hash");
    lfs_remove(&lfs, hash_path);  // best effort
  }

  SendRpcResponse(call_id, xbot::datatypes::RpcStatus::SUCCESS, &result, sizeof(result));
}

void FileService::RPCFileWrite(uint16_t call_id, const char* Path, uint32_t PathLen, uint32_t Offset,
                               const uint8_t* Data, uint32_t DataLen, uint32_t Hash) {
  char path[kMaxPath + 1];
  copy_path(Path, PathLen, path, sizeof(path));

  int32_t result = 0;

  do {
    File file;

    // Create parent directories (skip root-level paths, which need none).
    if (strchr(path + 1, '/') != nullptr) {
      int r = file.mkdirp(path);
      if (r != LFS_ERR_OK) {
        ULOG_WARNING("File: mkdirp '%s' failed (%d)", path, r);
        result = r;
        break;
      }
    }

    char tmp[kMaxPath + 5];
    strcpy(tmp, path);
    strcat(tmp, ".tmp");

    int flags = LFS_O_WRONLY | LFS_O_CREAT;
    if (Offset == 0U) {
      flags |= LFS_O_TRUNC;  // fresh upload: drop any stale .tmp remainder
    }
    int r = file.open(tmp, flags);
    if (r != LFS_ERR_OK) {
      ULOG_WARNING("File: open '%s' failed (%d)", tmp, r);
      result = r;
      break;
    }
    r = file.seek(static_cast<lfs_soff_t>(Offset), LFS_SEEK_SET);
    if (r < 0) {
      ULOG_WARNING("File: seek '%s' failed (%d)", tmp, r);
      result = r;
      break;
    }
    r = file.write(const_cast<uint8_t*>(Data), DataLen);
    if (r < 0) {
      ULOG_WARNING("File: write '%s' failed (%d)", tmp, r);
      result = r;
      break;
    }
    file.sync();
    file.close();
    result = r;  // bytes written

    if (Hash != 0U) {
      int finalize_result = finalize_file(path, Hash);
      if (finalize_result != LFS_ERR_OK) {
        ULOG_WARNING("File: finalize '%s' failed (%d)", path, finalize_result);
        result = finalize_result;
      }
    }
  } while (false);

  SendRpcResponse(call_id, xbot::datatypes::RpcStatus::SUCCESS, &result, sizeof(result));
}
