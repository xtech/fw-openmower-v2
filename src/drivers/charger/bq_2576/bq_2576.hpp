//
// Created by clemens on 04.09.24.
//

#ifndef BQ_2576_HPP
#define BQ_2576_HPP

#include <cstdint>
#include <drivers/charger/charger.hpp>

#include "hal.h"

class BQ2576 : public Charger {
 private:
  static constexpr uint8_t DEVICE_ADDRESS = 0x6B;
  static constexpr uint8_t REG_PART_INFORMATION = 0x3D;
  static constexpr uint8_t REG_TS_Charging_Region_Behavior_Control = 0x1C;
  static constexpr uint8_t REG_Power_Path_and_Reverse_Mode_Control = 0x19;
  static constexpr uint8_t REG_Fault_Status = 0x24;
  static constexpr uint8_t REG_IBAT_ADC = 0x2F;
  static constexpr uint8_t REG_ADC_Control = 0x2B;
  static constexpr uint8_t REG_VAC_ADC = 0x31;
  static constexpr uint8_t REG_VBAT_ADC = 0x33;
  static constexpr uint8_t REG_VFB_ADC = 0x39;
  static constexpr uint8_t REG_Charger_Control = 0x17;
  static constexpr uint8_t REG_Pin_Control = 0x18;
  static constexpr uint8_t REG_Charge_Current_Limit = 0x02;
  static constexpr uint8_t REG_Charger_Status_1 = 0x21;
  static constexpr uint8_t REG_Charger_Status_2 = 0x22;
  static constexpr uint8_t REG_Charger_Status_3 = 0x23;
  static constexpr uint8_t REG_Charger_Flag_1 = 0x25;
  static constexpr uint8_t REG_Charger_Flag_2 = 0x26;
  static constexpr uint8_t REG_Charger_Flag_3 = 0x27;
  static constexpr uint8_t REG_Gate_Driver_Dead_Time_Control = 0x3C;
  static constexpr uint8_t REG_ADC_Channel_Control = 0x2C;
  static constexpr uint8_t REG_Termination_Current_Limit = 0x12;
  static constexpr uint8_t REG_Gate_Driver_Strength_Control = 0x3B;
  static constexpr uint8_t REG_Precharge_Current_Limit = 0x10;
  static constexpr uint8_t REG_Precharge_and_Termination_Control = 0x14;

  bool readRegister(uint8_t reg, uint8_t &result);
  bool readRegister(uint8_t reg, uint16_t &result);
  bool writeRegister8(uint8_t reg, uint8_t value);
  bool writeRegister16(uint8_t reg, uint16_t value);

  bool getChargerStatus(uint8_t &status1, uint8_t &status2, uint8_t &status3);
  bool getChargerFlags(uint8_t &flags1, uint8_t &flags2, uint8_t &flags3);
  bool readVFB(float &result);

  uint8_t readFaults();

 public:
  bool setChargingCurrent(float current_amps, bool overwrite_hardware_limit) override;
  bool setPreChargeCurrent(float current_amps) override;
  bool setTerminationCurrent(float current_amps) override;

  CHARGER_STATUS getChargerStatus() override;
  bool init(I2CDriver *i2c_driver) override;
  bool resetWatchdog() override;

  bool setTsEnabled(bool enabled) override;

  bool readChargeCurrent(float &result) override;
  bool readAdapterVoltage(float &result) override;
  bool readBatteryVoltage(float &result) override;
};

#endif  // BQ_2576_HPP
