//
// Created by clemens on 10.11.24.
//

#ifndef DEBUGGABLE_DRIVER_HPP
#define DEBUGGABLE_DRIVER_HPP

#include <ch.h>
#include <etl/delegate.h>

#include <cstddef>
#include <cstdint>

/**
 * Class to be implemented by any driver which wants to provide raw data access for debugging (e.g. via TCP socket)
 */
class DebuggableDriver {
 public:
  typedef etl::delegate<void(const uint8_t *data, size_t size)> RawDataCallback;
  DebuggableDriver();
  virtual ~DebuggableDriver() = default;

  void SetRawAccessMode(bool enabled);
  virtual void RawDataInput(uint8_t *data, size_t size) = 0;
  void RawDataOutput(const uint8_t *data, size_t size);
  void SetRawDataCallback(const RawDataCallback &callback);

 protected:
  bool IsRawMode();
  MUTEX_DECL(mutex_);

 private:
  RawDataCallback raw_data_callback_{};
  bool is_raw_mode_ = false;
};

#endif  // DEBUGGABLE_DRIVER_HPP
