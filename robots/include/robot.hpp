#ifndef ROBOT_HPP
#define ROBOT_HPP

#include <drivers/motor/vesc/VescDriver.h>
#include <drivers/motor/yfr4esc/YFR4escDriver.h>
#include <hal.h>

#include <debug/debug_tcp_interface.hpp>

class Robot {
 public:
  virtual void InitPlatform() = 0;
  virtual bool IsHardwareSupported() = 0;

  virtual bool NeedsService(uint16_t id) {
    (void)id;
    return true;
  }

  virtual UARTDriver* GPS_GetUartPort() {
    // If nothing defined, we require a user setting.
    return nullptr;
  }

  /**
   * Return the default battery full voltage (i.e. this is considered 100% battery)
   */
  virtual float Power_GetDefaultBatteryFullVoltage() = 0;

  /**
   * Return the default battery empty voltage (i.e. this is considered 0% battery)
   */
  virtual float Power_GetDefaultBatteryEmptyVoltage() = 0;

  /**
   * Return the charging current for this robot
   */
  virtual float Power_GetDefaultChargeCurrent() = 0;

  /**
   * Return the minimum voltage before shutting down as much as possible
   */
  virtual float Power_GetAbsoluteMinVoltage() = 0;
};

class MowerRobot : public Robot {
 protected:
  void InitMotors();

  xbot::driver::motor::VescDriver left_motor_driver_{};
  xbot::driver::motor::VescDriver right_motor_driver_{};

  // Select mower motor ESC by platform: YFR4 on YardForce_V4, VESC otherwise
#if defined(ROBOT_PLATFORM_YardForce_V4)
  using MowerEscDriver = xbot::driver::motor::YFR4escDriver;
#else
  using MowerEscDriver = xbot::driver::motor::VescDriver;
#endif
  MowerEscDriver mower_motor_driver_{};

  DebugTCPInterface left_esc_driver_interface_{65102, &left_motor_driver_};
  DebugTCPInterface mower_esc_driver_interface_{65103, &mower_motor_driver_};
  DebugTCPInterface right_esc_driver_interface_{65104, &right_motor_driver_};
};

Robot* GetRobot();

#endif  // ROBOT_HPP
