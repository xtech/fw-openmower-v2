/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file versioned_struct.hpp
 * @brief Base template and utilities for versioned binary struct persistence
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-11-06
 */

#ifndef VERSIONED_STRUCT_HPP_
#define VERSIONED_STRUCT_HPP_

#include <ulog.h>

#include <cstdint>
#include <type_traits>

#include "file.hpp"

namespace xbot::driver::filesystem {

/**
 * @brief Helper macro to define version field at the start of a versioned struct
 *
 * This ensures the version field is always first and uses the correct type.
 */
#define VERSIONED_STRUCT_FIELDS(version_number)       \
  static constexpr uint16_t VERSION = version_number; \
  uint16_t version = VERSION

/**
 * @brief Base template for versioned structs
 *
 * Provides Load/Save methods for binary persistence with version migration support.
 * The derived struct must:
 * - Use VERSIONED_STRUCT_FIELDS(n) as first line in struct body
 * - Define static constexpr const char* PATH
 * - Be wrapped in #pragma pack(push, 1) / #pragma pack(pop)
 * - Have a static_assert for expected size
 *
 * Usage:
 * @code
 * #pragma pack(push, 1)
 * struct MySettings : public VersionedStruct<MySettings> {
 *   VERSIONED_STRUCT_FIELDS(1);  // Version 1
 *   static constexpr const char* PATH = "/cfg/my_settings.bin";
 *
 *   uint8_t some_value = 42;
 *   uint16_t other_value = 1000;
 * };
 * #pragma pack(pop)
 * static_assert(sizeof(MySettings) == 6);  // 2 bytes version + 4 bytes data
 * @endcode
 */
template <typename T>
struct VersionedStruct {
  /**
   * @brief Load struct from binary file with version migration support
   * @param dest Destination struct (should be pre-initialized with defaults)
   * @return true on success, false if file doesn't exist or version incompatible
   */
  static bool Load(T& dest) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "VersionedStruct must be trivially copyable for binary serialization");

    // Verify version field is at offset 0 with correct type
    static_assert(offsetof(T, version) == 0,
                  "Version field must be first in struct - use VERSIONED_STRUCT_FIELDS(n) as first line");
    static_assert(sizeof(dest.version) == sizeof(uint16_t),
                  "Version field must be uint16_t - use VERSIONED_STRUCT_FIELDS(n) macro");

    File file;

    int open_result = file.open(T::PATH, LFS_O_RDONLY);
    if (open_result != LFS_ERR_OK) {
      ULOG_ERROR("Failed to open %s for reading: error=%d", T::PATH, open_result);
      return false;  // File doesn't exist - caller should use defaults
    }

    // Read version field first (uint16_t = 2 bytes)
    uint16_t file_version;
    int read_result = file.read(&file_version, sizeof(file_version));
    if (read_result != sizeof(file_version)) {
      ULOG_ERROR("Failed to read %s version field: error=%d", T::PATH, read_result);
      return false;  // Read error
    }

    // Check version compatibility
    if (file_version > T::VERSION) {
      ULOG_ERROR("%s version %u is newer than current %u", T::PATH, file_version, T::VERSION);
      return false;  // File is from newer firmware - can't handle it
    }

    // Read remaining bytes directly into struct (after version field)
    lfs_soff_t remaining_bytes = file.size() - sizeof(file_version);
    file.seek(sizeof(file_version), LFS_SEEK_SET);

    if (remaining_bytes > 0) {
      // Read only what's available, directly into struct after version field
      char* dest_ptr = reinterpret_cast<char*>(&dest) + sizeof(file_version);
      size_t bytes_to_read = (static_cast<size_t>(remaining_bytes) < (sizeof(T) - sizeof(file_version)))
                                 ? static_cast<size_t>(remaining_bytes)
                                 : (sizeof(T) - sizeof(file_version));  // Ensure we don't overflow struct size

      read_result = file.read(dest_ptr, bytes_to_read);
      if (static_cast<size_t>(read_result) != bytes_to_read) {
        ULOG_ERROR("Failed to read %s data: error=%d", T::PATH, read_result);
        return false;  // Read error
      }
    }

    return true;
  }

  /**
   * @brief Save struct to binary file
   * @param src Source struct to save
   * @return true on success, false on write error
   */
  static bool Save(T& src) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "VersionedStruct must be trivially copyable for binary serialization");

    File file;

    // Create parent directories if needed (mkdirp handles file paths)
    int mkdirp_result = file.mkdirp(T::PATH);
    if (mkdirp_result != LFS_ERR_OK) {
      ULOG_ERROR("Failed to create parent directories for %s: error=%d", T::PATH, mkdirp_result);
      return mkdirp_result;
    }

    // Open file for writing
    int open_result = file.open(T::PATH, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (open_result != LFS_ERR_OK) {
      ULOG_ERROR("Failed to open %s for writing: error=%d", T::PATH, open_result);
      return open_result;
    }

    // Write binary struct directly
    int written = file.write(&src, sizeof(src));
    if (written != sizeof(src)) {
      ULOG_ERROR("Failed to write %s: wrote %d of %d bytes", T::PATH, written, sizeof(src));
      return false;
    }

    file.sync();
    return true;
  }
};

}  // namespace xbot::driver::filesystem

#endif  // VERSIONED_STRUCT_HPP_
