//
// Created by clemens on 29.09.24.
//

#ifndef HEARTBEAT_H
#define HEARTBEAT_H
#ifdef __cplusplus
extern "C" {
#endif
#define HEARTBEAT_RECENT_MS 100

/**
 * Initializes the timer task for showing the heartbeat (to show the RTOS hasn't
 * crashed)
 */
void InitHeartbeat(void);
#ifdef __cplusplus
}
#endif
#endif  // HEARTBEAT_H
