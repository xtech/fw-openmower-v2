#ifndef ROBOT_HPP
#define ROBOT_HPP

#include <drivers/motor/vesc/VescDriver.h>
#include <drivers/motor/yfr4esc/YFR4escDriver.h>
#include <hal.h>
#include <service_ids.h>

#include <debug/debug_tcp_interface.hpp>
#include <drivers/charger/charger.hpp>
#include <limits>

// Forward declare ProtocolType from GpsServiceBase.hpp
enum class ProtocolType : uint8_t;

class Robot {
 public:
  virtual ~Robot() = default;
  virtual void InitPlatform() = 0;
  // Hardware detection is static per-class:
  //   Phase 1 (unique hardware): static bool IsAutoDetected()
  //   Phase 2 (name-configured): static bool BoardIsCompatible() + static const char* FirmwareName()

  virtual bool NeedsService(uint16_t id) {
    return id != xbot::service_ids::BMS;  // BMS is opt-in, all other services are required by default
  }

  // Return false to skip GpioInputDriver registration (e.g. robots with dedicated input hardware).
  virtual bool NeedsGpioInputDriver() const {
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
   * Return the default maximum charging current for this robot.
   * This is usually for the provided power supply and battery.
   *
   * The user can override this value using the DangerouslyOverrideHardwareChargeCurrentLimit parameter.
   */
  virtual float Power_GetMaxChargeCurrent() = 0;

  /**
   * Return the charge voltage target (VREG) in Volts.
   * Returns -1.0 to leave the hardware-pin configured voltage unchanged.
   */
  virtual float Power_GetDefaultChargeVoltage() {
    return -1.0f;
  }

  /**
   * Return the termination current (ITERM) in Amps.
   * Charging is considered done when CV current drops below this value.
   */
  virtual float Power_GetDefaultTerminationCurrent() {
    return 0.250f;
  }

  /**
   * Return the pre-charge current (IPRECHG) in Amps.
   * Used when VBAT < VBAT_LOWV (pre-charge region).
   */
  virtual float Power_GetDefaultPreChargeCurrent() {
    return 0.250f;
  }

  virtual ChargerDriver::ReChargeVoltage Power_GetDefaultReChargeVoltage() {
    return ChargerDriver::ReChargeVoltage::PERCENT_97_6;
  }

  /**
   * Return the minimum voltage before shutting down as much as possible
   */
  virtual float Power_GetAbsoluteMinVoltage() = 0;

  /**
   * Set the max. allowed system current in Amps.
   * This is to limit e.g. the charger current for not overloading the wall power supply.
   * Only usefull for systems that do have some kind of system current sense.
   */
  virtual void Power_SetSystemCurrent(float system_current) {
    (void)system_current;
  };

  /**
   * Save GPS settings to persistent storage (optional).
   * Default implementation does nothing.
   * @param protocol GPS protocol (0=UBX, 1=NMEA as per ProtocolType enum)
   * @param uart UART index (0 = default robot port)
   * @param baudrate Baudrate
   */
  virtual bool SaveGpsSettings(ProtocolType protocol, uint8_t uart, uint32_t baudrate) {
    (void)protocol;
    (void)uart;
    (void)baudrate;
    return true;
  }
};

class MowerRobot : public Robot {
 protected:
  void InitMotors();
  virtual void InitMowerEsc();

  xbot::driver::motor::VescDriver left_motor_driver_{};
  xbot::driver::motor::VescDriver right_motor_driver_{};
  xbot::driver::motor::VescDriver mower_motor_driver_{};

  DebugTCPInterface left_esc_driver_interface_{65102, &left_motor_driver_};
  DebugTCPInterface mower_esc_driver_interface_{65103, &mower_motor_driver_};
  DebugTCPInterface right_esc_driver_interface_{65104, &right_motor_driver_};
};

// Stage 1: returns non-null if the carrier board uniquely identifies one robot.
Robot* TryAutoDetectRobot();

// True if the board is known but requires Stage 2 (MetaService Robot Firmware register).
bool BoardSupportsStage2();

// Stage 2: instantiate robot by firmware name, validated against the current board.
// Returns nullptr if name is invalid for this board.
Robot* GetRobotByName(const char* name);

#endif  // ROBOT_HPP
