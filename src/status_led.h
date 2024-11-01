//
// Created by clemens on 29.09.24.
//

#ifndef STATUS_LED_H
#define STATUS_LED_H

enum LED_MODE {
  LED_MODE_OFF,
  LED_MODE_ON,
  LED_MODE_BLINK_FAST,
  LED_MODE_BLINK_SLOW
};

void InitStatusLed(void);

void SetStatusLedMode(enum LED_MODE mode);

#endif  // STATUS_LED_H
