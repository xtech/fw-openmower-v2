//
// Created by clemens on 14.04.25.
//

#ifndef MOTOR_DRIVER_HPP
#define MOTOR_DRIVER_HPP

#include <cstdint>
#include <etl/delegate.h>


namespace xbot::driver::motor {
class MotorDriver {


public:
  struct ESCState {
    enum class ESCStatus : uint8_t {
      ESC_STATUS_DISCONNECTED = 99u,
      ESC_STATUS_ERROR = 100u,
      ESC_STATUS_STALLED = 150u,
      ESC_STATUS_OK = 200u,
      ESC_STATUS_RUNNING = 201u,
    };

    uint8_t fw_major;
    uint8_t fw_minor;

    float voltage_input;
    float temperature_pcb;
    float temperature_motor;
    float current_input;
    float duty_cycle;
    uint32_t tacho;
    uint32_t tacho_absolute;
    float direction;
    float rpm;

    ESCStatus status;
  };

  typedef etl::delegate<void(const ESCState &new_state)> StateCallback;

  virtual ~MotorDriver() = default;

  virtual void RequestStatus() = 0;
  virtual void SetDuty(float duty) = 0;

  virtual bool Start() {
    chDbgAssert(!started_, "Don't start twice");
    started_ = true;
    return true;
  }

  void SetStateCallback(const StateCallback &function) {
    chDbgAssert(!started_, "SetCallback before starting the driver!");
    state_callback_ = function;
  }

protected:

  bool IsStarted() {
    return started_;
  }

  void NotifyCallback() {
    chDbgAssert(started_, "Notify can only be called after driver started.");
    if (state_callback_) {
      state_callback_(latest_state_);
    }
  }
  ESCState latest_state_{};


private:
  bool started_ = false;
  StateCallback state_callback_{};
};
}


#endif //MOTOR_DRIVER_HPP
