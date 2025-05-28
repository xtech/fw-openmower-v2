//
// Created by clemens on 27.05.25.
//

#ifndef LFS_CONFIG_H
#define LFS_CONFIG_H

#include "ch.h"
#include "ulog.h"

// Dont allow malloc
#define LFS_NO_MALLOC

#define LFS_DEBUG ULOG_DEBUG
#define LFS_WARN ULOG_WARNING
#define LFS_ERROR ULOG_ERROR
#define LFS_ASSERT(test) chDbgAssert(test, "LFS Assert")

// Make it thread-safe
#define LFS_THREADSAFE

#endif //LFS_CONFIG_H
