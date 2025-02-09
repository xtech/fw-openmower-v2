//
// Created by clemens on 29.09.24.
//

#include "heartbeat.h"

#include "ch.h"
#include "hal.h"

// Gets updated by the idle task to the current tick value.
volatile uint32_t last_idle_tick = 0;

static volatile uint8_t blink_state = 0;

static virtual_timer_t heartbeat_timer;

static void heartbeat_timer_cb(struct ch_virtual_timer *tp, void *arg) {
  (void)arg;
  (void)tp;
  bool elapsed = TIME_I2MS(chVTTimeElapsedSinceX(last_idle_tick)) > HEARTBEAT_RECENT_MS;
  switch (blink_state) {
    case 0:
      if (elapsed) {
        palClearLine(LINE_HEARTBEAT_LED_RED);
        palSetLine(LINE_HEARTBEAT_LED_GREEN);
      } else {
        palClearLine(LINE_HEARTBEAT_LED_GREEN);
        palSetLine(LINE_HEARTBEAT_LED_RED);
      }
      chSysLockFromISR();
      chVTSetI(&heartbeat_timer, TIME_MS2I(100), heartbeat_timer_cb, NULL);
      chSysUnlockFromISR();
      blink_state++;
      break;
    case 1:
      palSetLine(LINE_HEARTBEAT_LED_GREEN);
      palSetLine(LINE_HEARTBEAT_LED_RED);
      chSysLockFromISR();
      chVTSetI(&heartbeat_timer, TIME_MS2I(100), heartbeat_timer_cb, NULL);
      chSysUnlockFromISR();
      blink_state++;
      break;
    case 2:
      if (elapsed) {
        palClearLine(LINE_HEARTBEAT_LED_RED);
        palSetLine(LINE_HEARTBEAT_LED_GREEN);
      } else {
        palClearLine(LINE_HEARTBEAT_LED_GREEN);
        palSetLine(LINE_HEARTBEAT_LED_RED);
      }
      chSysLockFromISR();
      chVTSetI(&heartbeat_timer, TIME_MS2I(100), heartbeat_timer_cb, NULL);
      chSysUnlockFromISR();
      blink_state++;
      break;
    default:
      palSetLine(LINE_HEARTBEAT_LED_GREEN);
      palSetLine(LINE_HEARTBEAT_LED_RED);

      chSysLockFromISR();
      chVTSetI(&heartbeat_timer, TIME_MS2I(500), heartbeat_timer_cb, NULL);
      chSysUnlockFromISR();
      blink_state = 0;
      break;
  }
}

void InitHeartbeat() {
  chVTObjectInit(&heartbeat_timer);
  chSysLock();
  chVTSetI(&heartbeat_timer, TIME_MS2I(100), heartbeat_timer_cb, NULL);
  chSysUnlock();
}
