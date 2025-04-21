//
// Created by Apehaenger on 4/20/25.
//
#include "sabo_ui_controller.hpp"

#include <ulog.h>

static constexpr uint8_t EVT_PACKET_RECEIVED = 1;

void SaboUIController::start() {
  if (thread_ != nullptr) {
    ULOG_ERROR("Started Sabo UI Controller twice!");
    return;
  }

  if (driver_ == nullptr) {
    ULOG_ERROR("Sabo UI Driver not set!");
    return;
  }
  if (!driver_->init()) {
    ULOG_ERROR("Failed to initialize Sabo UI Driver!");
    return;
  }

  thread_ = chThdCreateStatic(&wa_, sizeof(wa_), NORMALPRIO, ThreadHelper, this);
#ifdef USE_SEGGER_SYSTEMVIEW
  processing_thread_->name = "SaboUIController";
#endif

  // Now that driver is initialized and thread got started, we can enable output
  driver_->enableOutput();
}

void SaboUIController::ThreadHelper(void* instance) {
  auto* i = static_cast<SaboUIController*>(instance);
  i->ThreadFunc();
}

void SaboUIController::tick() {
  const systime_t now = chVTGetSystemTime();

  // Slow blink handling
  if (leds_.slow_blink_mask && (now - leds_.last_slow_update >= TIME_MS2I(500))) {
    leds_.last_slow_update = now;
    leds_.slow_blink_state = !leds_.slow_blink_state;
  }

  // Fast blink handling
  if (leds_.fast_blink_mask && (now - leds_.last_fast_update >= TIME_MS2I(100))) {
    leds_.last_fast_update = now;
    leds_.fast_blink_state = !leds_.fast_blink_state;
  }

  // Final LED state calculation
  uint8_t active_leds = leds_.on_mask;
  active_leds |= leds_.slow_blink_state ? leds_.slow_blink_mask : 0;
  active_leds |= leds_.fast_blink_state ? leds_.fast_blink_mask : 0;

  driver_->setLEDs(active_leds);
  driver_->latchLoad();

  // TODO: Add button reading or handling

  /* Debug stack size
  size_t stack_size = sizeof(wa_);
  size_t unused_stack = (uint8_t*)thread_->ctx.sp - (uint8_t*)&wa_[0];
  size_t used_stack = stack_size - unused_stack;
  ULOG_INFO("Stack: %u/%u used", used_stack, stack_size);*/
}

void SaboUIController::setLED(LEDID id, LEDMode mode) {
  const uint8_t bit = 1 << static_cast<uint8_t>(id);

  // Clear existing state
  leds_.on_mask &= ~bit;
  leds_.slow_blink_mask &= ~bit;
  leds_.fast_blink_mask &= ~bit;

  // Set new state
  switch (mode) {
    case LEDMode::ON: leds_.on_mask |= bit; break;
    case LEDMode::BLINK_SLOW: leds_.slow_blink_mask |= bit; break;
    case LEDMode::BLINK_FAST: leds_.fast_blink_mask |= bit; break;
    case LEDMode::OFF:
    default: break;
  }
}

void SaboUIController::ThreadFunc() {
  while (true) {
    tick();

    // FIXME: Sample code to wait for some other event
    eventmask_t event = chEvtWaitAnyTimeout(
        EVT_PACKET_RECEIVED,
        TIME_MS2I(100));  // FIXME: Once buttons are implemented, this needs to become something around 1ms
    if (event & EVT_PACKET_RECEIVED) {
      /*chSysLock();
      // Forbid packet reception
      processing = true;
      chSysUnlock();
      if (buffer_fill > 0) {
        ProcessPacket();
      }
      buffer_fill = 0;
      chSysLock();
      // Allow packet reception
      processing = false;
      chSysUnlock();*/
    }
  }
}
