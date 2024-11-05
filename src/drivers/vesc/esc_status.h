//
// Created by clemens on 05.11.24.
//

#ifndef ESC_STATUS_H
#define ESC_STATUS_H
enum {
  ESC_STATUS_DISCONNECTED = 99u,
  ESC_STATUS_ERROR = 100u,
  ESC_STATUS_STALLED = 150u,
  ESC_STATUS_OK = 200u,
  ESC_STATUS_RUNNING = 201u,
};
#endif  // ESC_STATUS_H
