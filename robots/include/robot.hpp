#ifndef ROBOT_HPP
#define ROBOT_HPP

#include <drivers/motor/vesc/VescDriver.h>
#include <drivers/motor/yfr4esc/YFR4escDriver.h>
#include <etl/array.h>
#include <etl/string_view.h>
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
    if (id == xbot::service_ids::SOUND) return HasSound();  // sound only where the board has a chip
    return id != xbot::service_ids::BMS;                    // BMS is opt-in, all other services are required by default
  }

  // Return true if this board has a sound chip (MAX98357A).
  // Sound is only initialized/used when true. Default false (opt-in).
  virtual bool HasSound() const {
    return false;
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
    return ChargerDriver::ReChargeVoltage::PERCENT_94_3;
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

  /**
   * @brief Maximum safe mechanical RPM for the mower motor.
   *
   * Some robots (e.g. Sabo) can reach dangerous RPMs when the VESC eRPM limit
   * is not configured. This value defines a firmware-enforced emergency cutoff
   * that immediately cuts mower power if exceeded.
   *
   * @return Mechanical RPM limit, or quiet_NaN() if no limit is needed (default).
   */
  virtual float GetMowerMaxSafeRpm() {
    return std::numeric_limits<float>::quiet_NaN();
  }
};

/**
 * Robots with mower motors (i.e. everything except xBot).
 *
 * The mower driver type is a template parameter, so exactly one mower driver (and one debug
 * interface) exists at runtime instead of an unused second one for the other variant:
 *   MowerRobotBase<VescDriver>    -> VESC mower (YardForce Std., Sabo, Universal, Worx, ...)
 *   MowerRobotBase<YFR4escDriver> -> YardForce-V4 (YardForce Rev4 mower ESC)
 *
 * All mower motors of a robot are the same type and are meant to run in parallel, so a robot
 * with more than one mower motor sets kMowerMotors accordingly (Universal variants are queued
 * for that).
 * IMPORTANT for whoever implements that: the MowerService still knows a single
 * driver - one speed input, one set of telemetry values and one health flag - so multi-motor
 * support needs a service definition change (mower_service.json + version bump) and a fan-out
 * in the service (apply the same duty to every motor, decide what to report for RPM/current/
 * temperature of several motors, check health per motor). Until then InitMowerEsc() refuses to
 * compile for kMowerMotors > 1 on purpose.
 */
template <typename MowerDriverT, size_t kMowerMotors = 1>
class MowerRobotBase : public Robot {
 protected:
  void InitMotors();
  void InitMowerEsc();

  xbot::driver::motor::VescDriver left_motor_driver_{};
  xbot::driver::motor::VescDriver right_motor_driver_{};
  etl::array<MowerDriverT, kMowerMotors> mower_motor_driver_{};

  DebugTCPInterface left_esc_driver_interface_{65102, &left_motor_driver_};
  DebugTCPInterface mower_esc_driver_interface_{65103, &mower_motor_driver_[0]};
  DebugTCPInterface right_esc_driver_interface_{65104, &right_motor_driver_};
};

// All VESC-mower robots (unchanged base for Sabo, Universal, Worx, Lyfco, Husq and the YardForce Std.
using MowerRobot = MowerRobotBase<xbot::driver::motor::VescDriver>;

// Stage 1: returns non-null if the carrier board uniquely identifies one robot.
Robot* TryAutoDetectRobot();

// True if the board is known but requires Stage 2 (MetaService Robot Firmware register).
bool BoardSupportsStage2();

// Stage 2: instantiate robot by firmware name, validated against the current board.
// Returns nullptr if name is invalid for this board.
Robot* GetRobotByName(etl::string_view name);

#endif  // ROBOT_HPP
