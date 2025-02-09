//
// Created by clemens on 04.09.24.
//

#include "bq_2576.hpp"

#include "ch.h"
#include "hal.h"

bool BQ2576::init(I2CDriver* i2c_driver) {
  this->i2c_driver_ = i2c_driver;
  uint8_t result = 0;

  bool readOk = readRegister(REG_PART_INFORMATION, result);
  bool charger_connected = readOk && (result & 0b01111000) == 0b010000;
  bool success = charger_connected;

  // Reset the charger settings
  {
    bool writeOk = writeRegister8(REG_Power_Path_and_Reverse_Mode_Control, 0b10100000);
    if (!writeOk) {
      return false;
    }
  }

  // Wait for reset done
  bool reset_done;
  do {
    // Wait 100ms
    chThdSleep(TIME_MS2I(100));
    readOk = readRegister(REG_Power_Path_and_Reverse_Mode_Control, result);
    if (!readOk) {
      return false;
    }
    reset_done = (result & 0b1000000) == 0;
  } while (!reset_done);

  // Disable PFM
  success &= writeRegister8(REG_Power_Path_and_Reverse_Mode_Control, 0);

  // Enable automatic ADC sampling
  success &= writeRegister8(REG_ADC_Control, 0b10000000);

  resetWatchdog();

  return success;
}
bool BQ2576::setTsEnabled(bool enabled) {
  uint8_t val;
  if (!readRegister(REG_TS_Charging_Region_Behavior_Control, val)) return false;
  if (enabled) {
    val |= 1;
  } else {
    val &= 0xFE;
  }
  return writeRegister8(REG_TS_Charging_Region_Behavior_Control, val);
}
uint8_t BQ2576::readFaults() {
  uint8_t result = 0;

  readRegister(REG_Fault_Status, result);
  return result;
}
bool BQ2576::readRegister(uint8_t reg, uint8_t& result) {
  i2cAcquireBus(i2c_driver_);
  bool ok = i2cMasterTransmit(i2c_driver_, DEVICE_ADDRESS, &reg, sizeof(reg), &result, sizeof(result)) == MSG_OK;
  i2cReleaseBus(i2c_driver_);
  return ok;
}
bool BQ2576::readChargeCurrent(float& result) {
  uint16_t raw_result = 0;
  if (!readRegister(REG_IBAT_ADC, raw_result)) return false;

  result = static_cast<int16_t>(raw_result) * 2.0f / 1000.0f;

  return true;
}
bool BQ2576::readRegister(uint8_t reg, uint16_t& result) {
  i2cAcquireBus(i2c_driver_);
  bool ok = i2cMasterTransmit(i2c_driver_, DEVICE_ADDRESS, &reg, sizeof(reg), reinterpret_cast<uint8_t*>(&result),
                              sizeof(result)) == MSG_OK;
  i2cReleaseBus(i2c_driver_);
  return ok;
}
bool BQ2576::writeRegister8(uint8_t reg, uint8_t value) {
  uint8_t payload[2] = {reg, value};
  i2cAcquireBus(i2c_driver_);
  bool ok = i2cMasterTransmit(i2c_driver_, DEVICE_ADDRESS, payload, sizeof(payload), nullptr, 0) == MSG_OK;
  i2cReleaseBus(i2c_driver_);
  return ok;
}
bool BQ2576::writeRegister16(uint8_t reg, uint16_t value) {
  const auto ptr = reinterpret_cast<uint8_t*>(&value);
  uint8_t payload[3] = {reg, ptr[0], ptr[1]};
  i2cAcquireBus(i2c_driver_);
  bool ok = i2cMasterTransmit(i2c_driver_, DEVICE_ADDRESS, payload, sizeof(payload), nullptr, 0) == MSG_OK;
  i2cReleaseBus(i2c_driver_);
  return ok;
}
bool BQ2576::readAdapterVoltage(float& result) {
  uint16_t raw_result = 0;
  if (!readRegister(REG_VAC_ADC, raw_result)) return false;

  // raw result is in 2mV steps
  result = static_cast<int16_t>(raw_result) * 2.0f / 1000.0f;

  return true;
}
bool BQ2576::readBatteryVoltage(float& result) {
  uint16_t raw_result = 0;
  if (!readRegister(REG_VBAT_ADC, raw_result)) return false;

  // raw result is in 2mV steps
  result = static_cast<int16_t>(raw_result) * 2.0f / 1000.0f;

  return true;
}
bool BQ2576::resetWatchdog() {
  // TODO, if the REG_Charger_Control is used, we need to either store the value
  // or read it here before resetting the watchdog.
  uint8_t val = 0b11101001;
  return writeRegister8(REG_Charger_Control, val);
}
bool BQ2576::getChargerStatus(uint8_t& status1, uint8_t& status2, uint8_t& status3) {
  bool success = true;
  success &= readRegister(REG_Charger_Status_1, status1);
  success &= readRegister(REG_Charger_Status_2, status2);
  success &= readRegister(REG_Charger_Status_3, status3);
  return success;
}
bool BQ2576::getChargerFlags(uint8_t& flags1, uint8_t& flags2, uint8_t& flags3) {
  bool success = true;
  success &= readRegister(REG_Charger_Flag_1, flags1);
  success &= readRegister(REG_Charger_Flag_2, flags2);
  success &= readRegister(REG_Charger_Flag_3, flags3);
  return success;
}
bool BQ2576::readVFB(float& result) {
  uint16_t raw_result = 0;
  if (!readRegister(REG_VFB_ADC, raw_result)) return false;

  // raw result is in 1mV steps
  result = raw_result / 1000.0f;

  return true;
}
bool BQ2576::setChargingCurrent(float current_amps, bool overwrite_hardware_limit) {
  uint8_t pin_ctl_value;
  if (!readRegister(REG_Pin_Control, pin_ctl_value)) {
    return false;
  }

  if (overwrite_hardware_limit) {
    // Force EN_ICHG_PIN bit low
    pin_ctl_value &= 0b01111111;
  } else {
    // Force EN_ICHG_PIN bit high
    pin_ctl_value |= 0b10000000;
  }
  // Set the pin
  if (!writeRegister8(REG_Pin_Control, pin_ctl_value)) return false;

  if (current_amps < 0.4f) current_amps = 0.4f;
  uint16_t value = static_cast<uint16_t>(current_amps * 1000.0f / 50.0f) << 2;
  return writeRegister16(REG_Charge_Current_Limit, value);
}
bool BQ2576::setPreChargeCurrent(float current_amps) {
  if (current_amps < 0.250f) current_amps = 0.250f;
  uint16_t value = static_cast<uint16_t>(current_amps * 1000.0f / 50.0f) << 2;
  return writeRegister16(REG_Precharge_Current_Limit, value);
}
bool BQ2576::setTerminationCurrent(float current_amps) {
  // Lower values are not allowed
  if (current_amps < 0.250f) current_amps = 0.250f;
  uint16_t value = static_cast<uint16_t>(current_amps * 1000.0f / 50.0f) << 2;
  return writeRegister16(REG_Termination_Current_Limit, value);
}
