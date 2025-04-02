//
// Created by clemens on 29.09.24.
//

#include "status_led.h"

#include "ch.h"
#include "hal.h"

static mutex_t status_led_mutex;
static enum LED_MODE current_mode;
static enum LED_COLOR current_color;

static volatile uint8_t blink_state = 0;

static virtual_timer_t status_led_timer;

static void status_led_timer_cb(struct ch_virtual_timer *tp, void *arg) {
  (void)arg;
  (void)tp;
  switch (current_mode) {
    case LED_MODE_OFF:
      palSetLine(LINE_STATUS_LED_RED);
      palSetLine(LINE_STATUS_LED_GREEN);
    break;
    case LED_MODE_ON:
      palWriteLine(LINE_STATUS_LED_RED, current_color == RED ? PAL_LOW : PAL_HIGH);
      palWriteLine(LINE_STATUS_LED_GREEN, current_color == GREEN ? PAL_LOW : PAL_HIGH);
    break;
    case LED_MODE_BLINK_FAST:
      if (current_color == RED) {
        palToggleLine(LINE_STATUS_LED_RED);
        palSetLine(LINE_STATUS_LED_GREEN);
      } else {
        palToggleLine(LINE_STATUS_LED_GREEN);
        palSetLine(LINE_STATUS_LED_RED);
      }
    break;
    case LED_MODE_BLINK_SLOW:
      if (blink_state++ % 10 == 0) {
        if (current_color == RED) {
          palToggleLine(LINE_STATUS_LED_RED);
          palSetLine(LINE_STATUS_LED_GREEN);
        } else {
          palToggleLine(LINE_STATUS_LED_GREEN);
          palSetLine(LINE_STATUS_LED_RED);
        }
      }
      break;
  }

  chSysLockFromISR();
  chVTSetI(&status_led_timer, TIME_MS2I(50), status_led_timer_cb, NULL);
  chSysUnlockFromISR();
}

void InitStatusLed() {
  chMtxObjectInit(&status_led_mutex);
  current_mode = LED_MODE_OFF;
  current_mode = RED;
  chSysLock();
  chVTSetI(&status_led_timer, TIME_MS2I(100), status_led_timer_cb, NULL);
  chSysUnlock();
}

void SetStatusLedMode(enum LED_MODE mode) {
  chMtxLock(&status_led_mutex);
  current_mode = mode;
  chMtxUnlock(&status_led_mutex);
}

void SetStatusLedColor(enum LED_COLOR color) {
  chMtxLock(&status_led_mutex);
  current_color = color;
  chMtxUnlock(&status_led_mutex);
}
