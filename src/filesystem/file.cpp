/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file file.cpp
 * @brief File handling utilities for LittleFS
 * @author Clemens
 * @date 2025-05-27
 * @contributor Apehaenger <joerg@ebeling.ws>
 * @modified 2025-11-05
 */

#include "file.hpp"

#include <cstring>

int File::mkdirp(const char* path) {
  if (path == nullptr || path[0] != '/') {
    return LFS_ERR_INVAL;
  }

  char temp_path[256];
  strncpy(temp_path, path, sizeof(temp_path) - 1);
  temp_path[sizeof(temp_path) - 1] = '\0';

  // Find the last slash to determine if this is a file path
  char* last_slash = nullptr;
  for (char* p = temp_path; *p; p++) {
    if (*p == '/') {
      last_slash = p;
    }
  }

  // If path contains a filename (last component after slash has no slash),
  // don't create it as a directory - only create parent directories (but skip the root '/')
  if (last_slash && last_slash > temp_path) {
    *last_slash = '\0';  // Temporarily cut off filename
  }

  // Create each parent directory
  for (char* p = temp_path + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';  // Temporarily terminate string
      int result = lfs_mkdir(&lfs, temp_path);
      if (result != LFS_ERR_OK && result != LFS_ERR_EXIST) {
        return result;  // Failed to create parent
      }
      *p = '/';  // Restore the slash
    }
  }

  // Create the final directory (unless we cut off a filename)
  int result = lfs_mkdir(&lfs, temp_path);
  if (result == LFS_ERR_EXIST) {
    return LFS_ERR_OK;  // Already exists is OK
  }
  return result;
}
