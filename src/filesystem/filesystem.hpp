//
// Created by clemens on 27.05.25.
//

#ifndef FILESYSTEM_HPP
#define FILESYSTEM_HPP

#include "lfs.h"

#define FS_CACHE_SIZE 256
#define FS_LOOKAHEAD_SIZE 256

extern lfs_t lfs;

bool InitFS();

#endif  // FILESYSTEM_HPP
