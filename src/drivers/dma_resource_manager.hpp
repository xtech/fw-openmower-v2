/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file dma_resource_manager.hpp
 * @brief DMA resource manager for prioritizing and sharing DMA streams among drivers
 * @author
 * @date 2026-01-17
 */

#ifndef DMA_RESOURCE_MANAGER_HPP
#define DMA_RESOURCE_MANAGER_HPP

#include <ch.h>
#include <etl/algorithm.h>
#include <etl/delegate.h>
#include <etl/vector.h>
#include <hal.h>

namespace xbot::driver {

/**
 * @brief State of a registered DMA resource
 */
enum class DmaResourceState : uint8_t {
  INACTIVE = 0,  ///< Driver is inactive, DMA streams can be freed
  ACTIVE = 1,    ///< Driver is actively using DMA streams
  PENDING = 2,   ///< Driver is waiting for DMA streams
};

/**
 * @brief Handle for a registered DMA resource
 */
struct DmaResourceHandle {
  void* key;                              ///< Opaque key (usually driver instance pointer)
  void* resource_id;                      ///< Identifier for shared hardware resource (e.g., I2CDriver*)
  uint8_t priority;                       ///< Priority (0 = highest, 255 = lowest)
  DmaResourceState state;                 ///< Current state
  etl::delegate<msg_t()> start_delegate;  ///< Delegate to start the driver
  etl::delegate<void()> stop_delegate;    ///< Delegate to stop the driver

  /// Comparison operator for sorting by priority (lower number = higher priority)
  bool operator<(const DmaResourceHandle& other) const {
    return priority < other.priority;
  }
};

/**
 * @brief DMA Resource Manager for prioritizing and sharing DMA streams
 *
 * This manager allows drivers to register themselves with a priority.
 * When DMA streams are exhausted, lower priority inactive drivers can be
 * stopped to free up streams for higher priority drivers.
 * The manager supports shared hardware resources (e.g., multiple drivers
 * using the same I2C peripheral) by reference counting active drivers
 * per resource_id.
 */
class DmaResourceManager {
 public:
  static constexpr size_t MAX_DRIVERS = 10;  ///< Maximum number of registered drivers

  /**
   * @brief Get the singleton instance
   */
  static DmaResourceManager& GetInstance() {
    static DmaResourceManager instance;
    return instance;
  }

  /**
   * @brief Register a driver with the resource manager
   *
   * @param key Opaque key (usually driver instance pointer) used to identify the driver
   * @param resource_id Identifier for shared hardware resource (e.g., I2CDriver*).
   *                    Drivers sharing the same hardware should use the same resource_id.
   * @param priority Priority (0 = highest, 255 = lowest)
   * @param start_delegate Delegate to start the driver
   * @param stop_delegate Delegate to stop the driver
   * @return true if registration successful, false if max resources reached
   */
  bool Register(void* key, void* resource_id, uint8_t priority, etl::delegate<msg_t()> start_delegate,
                etl::delegate<void()> stop_delegate);

  /**
   * @brief Unregister a driver from the resource manager
   *
   * @param key Opaque key used during registration
   * @return true if driver was found and removed, false otherwise
   */
  bool Unregister(void* key);

  /**
   * @brief Request DMA resources for a driver
   *
   * Attempts to start the driver. If DMA streams are exhausted,
   * will try to stop lower priority inactive drivers to free streams.
   *
   * @param key Opaque key used during registration
   * @param max_retries Maximum number of retry attempts
   * @param retry_interval_ms Delay between retries in milliseconds
   * @return msg_t HAL_RET_SUCCESS if started, error code otherwise
   */
  msg_t Request(void* key, int max_retries = 5, systime_t retry_interval_ms = TIME_MS2I(10));

  /**
   * @brief Release DMA resources (mark as inactive)
   *
   * Marks the driver as inactive but does not stop it.
   * DMA streams remain allocated until needed by other drivers.
   *
   * @param key Opaque key used during registration
   */
  void Release(void* key);

  /**
   * @brief Stop driver and free DMA resources
   *
   * Stops the driver and marks it as inactive, freeing DMA streams.
   *
   * @param key Opaque key used during registration
   */
  void Stop(void* key);

  /**
   * @brief Get current state of a driver
   *
   * @param key Opaque key used during registration
   * @return DmaResourceState Current state, or INACTIVE if not found
   */
  DmaResourceState GetState(void* key) const;

  /**
   * @brief Get number of registered drivers
   */
  size_t GetDriverCount() const {
    return drivers_.size();
  }

  /**
   * @brief Get number of active drivers
   */
  size_t GetActiveCount() const;

 private:
  DmaResourceManager() = default;
  ~DmaResourceManager() = default;
  DmaResourceManager(const DmaResourceManager&) = delete;
  DmaResourceManager& operator=(const DmaResourceManager&) = delete;

  /**
   * @brief Find resource handle by key
   *
   * @param key Opaque key
   * @return DmaResourceHandle* Pointer to handle, or nullptr if not found
   */
  DmaResourceHandle* FindHandle(void* key);
  const DmaResourceHandle* FindHandle(void* key) const;

  /**
   * @brief Count active drivers sharing a given hardware resource
   *
   * @param resource_id Hardware resource identifier
   * @param exclude_key Optional driver key to exclude from count (e.g., the driver being considered for stopping)
   * @return size_t Number of active drivers using this hardware resource
   */
  size_t CountActiveDriversForResource(void* resource_id, void* exclude_key = nullptr) const;

  /**
   * @brief Try to free DMA streams by stopping lower priority inactive drivers
   *
   * @param requesting_priority Priority of the requesting driver
   * @param requesting_resource_id Hardware resource ID of the requesting driver (used to avoid stopping shared
   * resources)
   * @return true if at least one driver was stopped, false otherwise
   */
  bool FreeStreamsByPriority(uint8_t requesting_priority, void* requesting_resource_id);

  etl::vector<DmaResourceHandle, MAX_DRIVERS> drivers_;
};

/**
 * @brief RAII wrapper for DMA resource acquisition
 */
class DmaResourceGuard {
 public:
  /**
   * @brief Construct guard and request DMA resources
   *
   * @param key Opaque key used during registration
   * @param max_retries Maximum retry attempts
   * @param retry_interval_ms Delay between retries
   */
  DmaResourceGuard(void* key, int max_retries = 5, systime_t retry_interval_ms = TIME_MS2I(10))
      : key_(key), acquired_(false) {
    if (DmaResourceManager::GetInstance().Request(key, max_retries, retry_interval_ms) == HAL_RET_SUCCESS) {
      acquired_ = true;
    }
  }

  /**
   * @brief Destructor releases resources if acquired
   */
  ~DmaResourceGuard() {
    if (acquired_) {
      DmaResourceManager::GetInstance().Release(key_);
    }
  }

  /**
   * @brief Check if resources were successfully acquired
   */
  bool IsAcquired() const {
    return acquired_;
  }

  // Non-copyable
  DmaResourceGuard(const DmaResourceGuard&) = delete;
  DmaResourceGuard& operator=(const DmaResourceGuard&) = delete;

 private:
  void* key_;
  bool acquired_;
};

}  // namespace xbot::driver

#endif  // DMA_RESOURCE_MANAGER_HPP
