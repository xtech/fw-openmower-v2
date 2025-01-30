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

namespace Power {

I2CDriver* GetPowerI2CD() {
  return &I2CD1;
}

float GetMaxVoltage() {
  return 7.0f*4.2f;
}
float GetChargeCurrent() {
  return 0.5;
}
float GetMinVoltage() {
  // 3.3V min, 7s pack
  return 7.0f * 3.3;
}

}
}
