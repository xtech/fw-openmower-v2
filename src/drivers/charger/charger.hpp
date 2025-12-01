//
// Created by clemens on 02.04.25.
//

#ifndef CHARGER_HPP
#define CHARGER_HPP

#include <hal.h>

class ChargerDriver {
 protected:
  I2CDriver *i2c_driver_ = nullptr;

 public:
  enum class CHARGER_STATUS : uint8_t {
    NOT_CHARGING = 0,
    TRICKLE,
    PRE_CHARGE,
    CC,
    CV,
    TOP_OFF,
    DONE,
    FAULT,
    COMMS_ERROR,
    UNKNOWN
  };

  static constexpr const char *CHARGER_STATUS_STRINGS[] = {
      "Not Charging",       // NOT_CHARGING
      "Trickle Charge",     // TRICKLE
      "Pre Charge",         // PRE_CHARGE
      "Fast Charge (CC)",   // CC
      "Taper Charge (CV)",  // CV
      "Top Off",            // TOP_OFF
      "Done",               // DONE
      "Fault",              // FAULT
      "Comms Error",        // COMMS_ERROR
      "Unknown"             // UNKNOWN
  };

  static_assert(sizeof(CHARGER_STATUS_STRINGS) / sizeof(CHARGER_STATUS_STRINGS[0]) ==
                    static_cast<size_t>(CHARGER_STATUS::UNKNOWN) + 1,
                "CHARGER_STATUS_STRINGS size must match CHARGER_STATUS enum count");

  virtual ~ChargerDriver() = default;
  virtual bool setChargingCurrent(float current_amps, bool overwrite_hardware_limit) = 0;
  virtual bool setPreChargeCurrent(float current_amps) = 0;
  virtual bool setTerminationCurrent(float current_amps) = 0;
  virtual CHARGER_STATUS getChargerStatus() = 0;
  virtual bool init() = 0;
  virtual bool resetWatchdog() = 0;
  virtual bool setTsEnabled(bool enabled) = 0;
  virtual bool readChargeCurrent(float &result) = 0;
  virtual bool readAdapterVoltage(float &result) = 0;
  virtual bool readBatteryVoltage(float &result) = 0;

  void setI2C(I2CDriver *i2c) {
    i2c_driver_ = i2c;
  }

  static constexpr const char *statusToString(CHARGER_STATUS status) {
    return CHARGER_STATUS_STRINGS[static_cast<size_t>(status)];
  }
};

#endif  // CHARGER_HPP
