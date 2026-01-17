/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "dma_resource_manager.hpp"

#include <ch.h>
#include <ulog.h>

namespace xbot::driver {

bool DmaResourceManager::Register(void* key, void* resource_id, uint8_t priority, etl::delegate<msg_t()> start_delegate,
                                  etl::delegate<void()> stop_delegate) {
  if (drivers_.full()) {
    ULOG_ERROR("DmaResourceManager: Cannot register key %p, max drivers reached", key);
    return false;
  }

  // Check if already registered
  if (FindHandle(key) != nullptr) {
    ULOG_WARNING("DmaResourceManager: Key %p already registered", key);
    return false;
  }

  DmaResourceHandle handle;
  handle.key = key;
  handle.resource_id = resource_id;
  handle.priority = priority;
  handle.state = DmaResourceState::INACTIVE;
  handle.start_delegate = start_delegate;
  handle.stop_delegate = stop_delegate;

  drivers_.push_back(handle);
  // Keep sorted by priority (lower number = higher priority)
  etl::sort(drivers_.begin(), drivers_.end());

  ULOG_INFO("DmaResourceManager: Registered driver key %p with hardware resource %p priority %u", key, resource_id,
            priority);
  return true;
}

bool DmaResourceManager::Unregister(void* key) {
  auto it = etl::find_if(drivers_.begin(), drivers_.end(), [key](const DmaResourceHandle& h) { return h.key == key; });
  if (it == drivers_.end()) {
    ULOG_WARNING("DmaResourceManager: Key %p not found for unregister", key);
    return false;
  }

  // If driver is active, stop it first (but only if no other active drivers share the resource)
  if (it->state == DmaResourceState::ACTIVE && it->stop_delegate.is_valid()) {
    // Check if other active drivers share the same resource
    if (CountActiveDriversForResource(it->resource_id, it->key) == 0) {
      it->stop_delegate();
    }
  }

  drivers_.erase(it);
  ULOG_INFO("DmaResourceManager: Unregistered driver key %p", key);
  return true;
}

msg_t DmaResourceManager::Request(void* key, int max_retries, systime_t retry_interval_ms) {
  DmaResourceHandle* handle = FindHandle(key);
  if (handle == nullptr) {
    ULOG_ERROR("DmaResourceManager: Key %p not registered", key);
    return HAL_RET_NO_RESOURCE;
  }

  // If already active, return success
  if (handle->state == DmaResourceState::ACTIVE) {
    return HAL_RET_SUCCESS;
  }

  // Try to start the driver
  for (int attempt = 0; attempt < max_retries; ++attempt) {
    if (!handle->start_delegate.is_valid()) {
      ULOG_ERROR("DmaResourceManager: Invalid start delegate for key %p", key);
      return HAL_RET_NO_RESOURCE;
    }
    msg_t result = handle->start_delegate();
    if (result == HAL_RET_SUCCESS) {
      handle->state = DmaResourceState::ACTIVE;
      ULOG_DEBUG("DmaResourceManager: Driver key %p started successfully", key);
      return HAL_RET_SUCCESS;
    }

    if (result != HAL_RET_NO_RESOURCE) {
      // Some other error, don't retry
      ULOG_WARNING("DmaResourceManager: Driver key %p start failed with error %d", key, result);
      return result;
    }

    // DMA resource exhausted, try to free streams from lower priority inactive drivers
    if (FreeStreamsByPriority(handle->priority, handle->resource_id)) {
      // Streams freed, retry immediately
      continue;
    }

    // No streams could be freed, wait and retry
    if (attempt < max_retries - 1) {
      chThdSleep(retry_interval_ms);
    }
  }

  ULOG_WARNING("DmaResourceManager: Driver key %p failed to start after %d retries", key, max_retries);
  return HAL_RET_NO_RESOURCE;
}

void DmaResourceManager::Release(void* key) {
  DmaResourceHandle* handle = FindHandle(key);
  if (handle == nullptr) {
    ULOG_WARNING("DmaResourceManager: Key %p not registered", key);
    return;
  }

  if (handle->state == DmaResourceState::ACTIVE) {
    handle->state = DmaResourceState::INACTIVE;
    ULOG_DEBUG("DmaResourceManager: Driver key %p marked as inactive", key);
  }
}

void DmaResourceManager::Stop(void* key) {
  DmaResourceHandle* handle = FindHandle(key);
  if (handle == nullptr) {
    ULOG_WARNING("DmaResourceManager: Key %p not registered", key);
    return;
  }

  if (handle->state == DmaResourceState::ACTIVE && handle->stop_delegate.is_valid()) {
    // Only stop if no other active drivers share the resource
    if (CountActiveDriversForResource(handle->resource_id, handle->key) == 0) {
      handle->stop_delegate();
    }
  }
  handle->state = DmaResourceState::INACTIVE;
  ULOG_DEBUG("DmaResourceManager: Driver key %p stopped", key);
}

DmaResourceState DmaResourceManager::GetState(void* key) const {
  const DmaResourceHandle* handle = FindHandle(key);
  if (handle == nullptr) {
    return DmaResourceState::INACTIVE;
  }
  return handle->state;
}

size_t DmaResourceManager::GetActiveCount() const {
  size_t count = 0;
  for (const auto& handle : drivers_) {
    if (handle.state == DmaResourceState::ACTIVE) {
      ++count;
    }
  }
  return count;
}

DmaResourceHandle* DmaResourceManager::FindHandle(void* key) {
  auto it = etl::find_if(drivers_.begin(), drivers_.end(), [key](const DmaResourceHandle& h) { return h.key == key; });
  return (it != drivers_.end()) ? &(*it) : nullptr;
}

const DmaResourceHandle* DmaResourceManager::FindHandle(void* key) const {
  auto it = etl::find_if(drivers_.begin(), drivers_.end(), [key](const DmaResourceHandle& h) { return h.key == key; });
  return (it != drivers_.end()) ? &(*it) : nullptr;
}

size_t DmaResourceManager::CountActiveDriversForResource(void* resource_id, void* exclude_key) const {
  size_t count = 0;
  for (const auto& handle : drivers_) {
    if (handle.resource_id == resource_id && handle.state == DmaResourceState::ACTIVE && handle.key != exclude_key) {
      ++count;
    }
  }
  return count;
}

bool DmaResourceManager::FreeStreamsByPriority(uint8_t requesting_priority, void* requesting_resource_id) {
  // Iterate from lowest priority to highest (reverse order)
  for (auto it = drivers_.rbegin(); it != drivers_.rend(); ++it) {
    if (it->priority > requesting_priority &&  // Lower priority (higher number)
        it->state == DmaResourceState::INACTIVE && it->stop_delegate.is_valid()) {
      // Skip if the inactive driver shares the same resource with the requesting driver
      // (they would be using the same hardware, stopping it would affect the requester)
      if (it->resource_id == requesting_resource_id) {
        continue;
      }
      // Check if other active drivers share the same resource
      if (CountActiveDriversForResource(it->resource_id, it->key) > 0) {
        // There are other active drivers using this resource, stopping would affect them
        continue;
      }
      // No other active drivers, safe to stop
      ULOG_DEBUG(
          "DmaResourceManager: Stopping inactive driver key %p (priority %u, hardware resource %p) to free DMA streams",
          it->key, it->priority, it->resource_id);
      it->stop_delegate();
      return true;
    }
  }
  return false;
}

}  // namespace xbot::driver
