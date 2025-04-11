//
// Created by clemens on 4/11/25.
//

#include "gpio_emergency_driver.hpp"

#include "globals.hpp"
#include "hal.h"
#include "ulog.h"

static constexpr eventmask_t EVENT_GPIO_CHANGED = 1;

void GPIOEmergencyDriver::Start() {
  if(thread_ != nullptr) {
    ULOG_ERROR("Started GPIO Emergency Driver twice!");
    return;
  }
  thread_ = chThdCreateStatic(&wa_, sizeof(wa_), NORMALPRIO, ThreadHelper, this);
#ifdef USE_SEGGER_SYSTEMVIEW
  processing_thread_->name = "VESCDriver";
#endif
}
void GPIOEmergencyDriver::ThreadHelper(void *instance) {
  auto* i = static_cast<GPIOEmergencyDriver*>(instance);
  i->ThreadFunc();
}
void GPIOEmergencyDriver::ThreadFunc() {
  while(true) {
    bool emergency_detected = false;
    sysinterval_t wait_time = TIME_S2I(1);
    chMtxLock(&mtx_);
    for(auto &input : inputs_) {
      bool active_now = (palReadLine(input.gpio_line) == PAL_HIGH) ^ input.invert;
      if(active_now) {
        wait_time = TIME_MS2I(100);
        if (!input.active) {
          // track, the start
          input.active = true;
          input.active_since = chVTGetSystemTime();
        } else {
          // already active and still active
          if (chVTTimeElapsedSinceX(input.active_since) > input.timeout_duration) {
            // Trigger Emergency
            emergency_detected = true;
          }
        }
      } else {
        input.active = false;
      }
    }
    chMtxUnlock(&mtx_);

    const auto status = GetMowerStatus();
    if(emergency_detected) {
      if(!status.emergency_active || !status.emergency_latch) {
        const auto cb = [](MowerStatus& mower_status) {
          mower_status.emergency_active = true;
          mower_status.emergency_latch = true;
        };
        UpdateMowerStatus(cb);
        chEvtBroadcastFlags(&mower_events, MowerEvents::EMERGENCY_CHANGED);
      }
    } else {
      if(status.emergency_active) {
        const auto cb = [](MowerStatus& mower_status) {
          mower_status.emergency_active = false;
        };
        UpdateMowerStatus(cb);
        chEvtBroadcastFlags(&mower_events, MowerEvents::EMERGENCY_CHANGED);
      }
    }
    // Wait for up to 1 sec, interrupt earlier on GPIO change
    chEvtWaitAnyTimeout(EVENT_GPIO_CHANGED, wait_time);
  }
}
void GPIOEmergencyDriver::AddInput(const GPIOEmergencyDriver::Input& input) {
  chMtxLock(&mtx_);
  chDbgAssert(!inputs_.full(), "Cannot add more inputs!");
  if(!inputs_.full()) {
    inputs_.push_back(input);
    palSetLineMode(input.gpio_line, PAL_MODE_INPUT);
    palSetLineCallback(input.gpio_line, GPIOEmergencyDriver::GPIOCallback, this);
    palEnableLineEvent(input.gpio_line, PAL_EVENT_MODE_BOTH_EDGES);
  }
  chMtxUnlock(&mtx_);
}
void GPIOEmergencyDriver::GPIOCallback(void* instance) {
  auto* i = static_cast<GPIOEmergencyDriver*>(instance);
  if(i->thread_) {
    chSysLockFromISR();
    chEvtSignalI(i->thread_, EVENT_GPIO_CHANGED);
    chSysUnlockFromISR();
  }
}
