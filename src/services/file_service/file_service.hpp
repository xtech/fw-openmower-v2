/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file file_service.hpp
 * @brief Generic LittleFS file-transfer service (chunked upload + content hash).
 *
 * @note  Provides versioned file storage for the high-level system. The content
 *        hash is an opaque uint32_t computed by the caller; this service only
 *        stores it next to the file as a sidecar (".hash").
 */

#ifndef FILE_SERVICE_HPP
#define FILE_SERVICE_HPP

#include <FileServiceBase.hpp>

#include "filesystem/file.hpp"

class FileService : public FileServiceBase {
 public:
  /** Maximum path length accepted by the RPC layer (see file_service.json). */
  static constexpr size_t kMaxPath = 128U;

  explicit FileService(uint16_t service_id) : FileServiceBase(service_id, wa, sizeof(wa)) {
  }

 protected:
  void RPCFileExists(uint16_t call_id, const uint8_t* Path, uint32_t PathLen, uint32_t Hash) override;
  void RPCFileRemove(uint16_t call_id, const uint8_t* Path, uint32_t PathLen) override;
  void RPCFileWrite(uint16_t call_id, const uint8_t* Path, uint32_t PathLen, uint32_t Offset, const uint8_t* Data,
                    uint32_t DataLen, uint32_t Hash) override;
  void RPCFileList(uint16_t call_id, const uint8_t* Path, uint32_t PathLen, uint32_t StartIndex, uint8_t* data,
                   uint16_t* response_length) override;
  void RPCFileStats(uint16_t call_id, uint32_t* data, uint16_t* response_length) override;

  /** A claim restarts the service, so drop the upload that belongs to the old owner. */
  void OnStop() override;

 private:
  /**
   * @brief Staging file of the upload in progress.
   *
   * Kept open between the chunks of one upload: only the first chunk pays for
   * open() and only the last one for sync(), which is what makes a large transfer
   * cheap on flash. It is a member and not a local of the RPC because the file
   * cache (FS_CACHE_SIZE) would otherwise sit on the service thread stack.
   */
  File upload_;
  char upload_path_[kMaxPath + 1]{};
  uint32_t upload_written_ = 0;

  /** Forget the current upload: closes the staging file, keeps it on flash. */
  void abort_upload();
  // LittleFS directory commits (lfs_rename -> lfs_dir_commit -> compact/traverse)
  // are stack-hungry; 3072 bytes overflowed (MemManage fault). Measured peak is
  // 3388 bytes (thread watermark), so 4096 leaves ~20% headroom.
  THD_WORKING_AREA(wa, 4096){};
};

#endif  // FILE_SERVICE_HPP
