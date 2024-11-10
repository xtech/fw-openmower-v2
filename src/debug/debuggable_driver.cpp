//
// Created by clemens on 10.11.24.
//
#include <debug/debuggable_driver.hpp>
DebuggableDriver::DebuggableDriver() = default;
void DebuggableDriver::SetRawAccessMode(bool enabled) {
  chMtxLock(&mutex_);
  this->is_raw_mode_ = enabled;
  chMtxUnlock(&mutex_);
}
void DebuggableDriver::RawDataOutput(const uint8_t* data, size_t size) {
  chMtxLock(&mutex_);
  if (raw_data_callback_) {
    raw_data_callback_(data, size);
  }
  chMtxUnlock(&mutex_);
}
void DebuggableDriver::SetRawDataCallback(const RawDataCallback& callback) {
  chMtxLock(&mutex_);
  this->raw_data_callback_ = callback;
  chMtxUnlock(&mutex_);
}
bool DebuggableDriver::IsRawMode() {
  chMtxLock(&mutex_);
  bool cpy = is_raw_mode_;
  chMtxUnlock(&mutex_);
  return cpy;
}
