//
// Created by clemens on 29.09.24.
//

#ifndef STATUS_LED_H
#define STATUS_LED_H
#ifdef __cplusplus
extern "C" {
#endif
enum LED_MODE { LED_MODE_OFF, LED_MODE_ON, LED_MODE_BLINK_FAST, LED_MODE_BLINK_SLOW };
enum LED_COLOR { RED, GREEN };

void InitStatusLed(void);

void SetStatusLedMode(enum LED_MODE mode);
void SetStatusLedColor(enum LED_COLOR color);
#ifdef __cplusplus
}
#endif
#endif  // STATUS_LED_H
