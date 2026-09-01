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

class FileService : public FileServiceBase {
 public:
  explicit FileService(uint16_t service_id) : FileServiceBase(service_id, wa, sizeof(wa)) {
  }

 protected:
  void RPCFileExists(uint16_t call_id, const char* Path, uint32_t PathLen, uint32_t Hash) override;
  void RPCFileRemove(uint16_t call_id, const char* Path, uint32_t PathLen) override;
  void RPCFileWrite(uint16_t call_id, const char* Path, uint32_t PathLen, uint32_t Offset, const uint8_t* Data,
                    uint32_t DataLen, uint32_t Hash) override;

 private:
  THD_WORKING_AREA(wa, 3072){};
};

#endif  // FILE_SERVICE_HPP
