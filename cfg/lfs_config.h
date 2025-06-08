//
// Created by clemens on 27.05.25.
//

#ifndef LFS_CONFIG_H
#define LFS_CONFIG_H

#include "ch.h"

// Dont allow malloc
#define LFS_NO_MALLOC

// DO NOT redirect to ULOG
// because of a possible deadlock when using filesystem from TCPIP thread
// (e.g. tftp server uses tcp thread. if a log should be emitted, then the tcp thread waits for itself to emit that log)
#define LFS_TRACE(...)
#define LFS_DEBUG(...)
#define LFS_WARN(...)
#define LFS_ERROR(...)
#define LFS_ASSERT(test) chDbgAssert(test, "LFS Assert")

// Make it thread-safe
#define LFS_THREADSAFE

#endif //LFS_CONFIG_H
