//
// Created by clemens on 27.01.25.
//

#include "robot.hpp"

namespace Robot {

namespace General {
void InitPlatform() {
  // Not used, we could star the GUI driver task here for example
}
}

namespace Gps {
UARTDriver* GetUart() {
  return &UARTD8;
}
}

namespace Power {

I2CDriver* GetPowerI2CD() {
  return &I2CD1;
}

float GetMaxVoltage() {
  return 5.0f*4.2f;
}
float GetChargeCurrent() {
  return 0.5;
}
float GetMinVoltage() {
  // 3.3V min, 5s pack
  return 5.0f * 3.3;
}

}
}
