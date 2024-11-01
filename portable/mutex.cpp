//
// Created by clemens on 3/20/24.
//
#include <ch.h>

#include <xbot-service/portable/mutex.hpp>

bool xbot::service::mutex::initialize(MutexPtr mutex) {
  chMtxObjectInit(mutex);
  return true;
}

void xbot::service::mutex::deinitialize(MutexPtr mutex) {}

void xbot::service::mutex::lockMutex(MutexPtr mutex) { chMtxLock(mutex); }

void xbot::service::mutex::unlockMutex(MutexPtr mutex) { chMtxUnlock(mutex); }
