//
// Created by clemens on 03.04.25.
//

#include "bq_2579.hpp"
BQ2579::~BQ2579() = default;

bool BQ2579::setChargingCurrent(float current_amps, bool overwrite_hardware_limit) {
  (void)current_amps;
  (void)overwrite_hardware_limit;
  return true;
  return false;
}
bool BQ2579::setPreChargeCurrent(float current_amps) {
  (void)current_amps;
  return true;
  return false;
}
bool BQ2579::setTerminationCurrent(float current_amps) {
  (void)current_amps;
  return true;
  return false;
}
CHARGER_STATUS BQ2579::getChargerStatus() {
  return CHARGER_STATUS::NOT_CHARGING;
}
bool BQ2579::init(I2CDriver* i2c_driver) {
  this->i2c_driver_ = i2c_driver;

  if (!writeRegister8(REG_ADC_Control, 0b10000000)) {
    return false;
  }

  return true;
}
bool BQ2579::resetWatchdog() {
  uint8_t register_value = 0;
  if (!readRegister(REG_Charger_Control_1, register_value)) {
    return false;
  }

  register_value |= 0b00001000;
  return writeRegister8(REG_Charger_Control_1, register_value);
}
bool BQ2579::setTsEnabled(bool enabled) {
  uint8_t register_value = 0;
  if (!readRegister(REG_NTC_Control_1, register_value)) {
    return false;
  }

  if (!enabled) {
    // set IGNORE bit
    register_value |= 0b1;
  } else {
    // clear IGNORE bit
    register_value &= 0b11111110;
  }
  return writeRegister8(REG_NTC_Control_1, register_value);
}
bool BQ2579::readChargeCurrent(float& result) {
  uint16_t raw_result = 0;
  if (!readRegister(REG_IBAT_ADC, raw_result))
    return false;
  result = static_cast<float>(raw_result)/1000.0f;
  return true;
}
bool BQ2579::readAdapterVoltage(float& result) {
  uint16_t raw_result = 0;
  if (!readRegister(REG_VBUS_ADC, raw_result))
    return false;
  result = static_cast<float>(raw_result)/1000.0f;
  return true;
}
bool BQ2579::readBatteryVoltage(float& result) {
  uint16_t raw_result = 0;
  if (!readRegister(REG_VBAT_ADC, raw_result))
    return false;
  result = static_cast<float>(raw_result)/1000.0f;
  return true;
}

bool BQ2579::readSystemVoltage(float& result) {
  uint16_t raw_result = 0;
  if (!readRegister(REG_VSYS_ADC, raw_result))
    return false;
  result = static_cast<float>(raw_result)/1000.0f;
  return true;
}

bool BQ2579::readRegister(uint8_t reg, uint8_t& result) {
  i2cAcquireBus(i2c_driver_);
  bool ok = i2cMasterTransmit(i2c_driver_, DEVICE_ADDRESS, &reg, sizeof(reg), &result, sizeof(result)) == MSG_OK;
  i2cReleaseBus(i2c_driver_);
  return ok;
}
bool BQ2579::readRegister(uint8_t reg, uint16_t& result) {
  i2cAcquireBus(i2c_driver_);
  uint8_t buf[2];
  bool ok = i2cMasterTransmit(i2c_driver_, DEVICE_ADDRESS, &reg, sizeof(reg), buf,
                              sizeof(buf)) == MSG_OK;
  result = buf[0] << 8 | buf[1];
  i2cReleaseBus(i2c_driver_);
  return ok;
}
bool BQ2579::writeRegister8(uint8_t reg, uint8_t value) {
  uint8_t payload[2] = {reg, value};
  i2cAcquireBus(i2c_driver_);
  bool ok = i2cMasterTransmit(i2c_driver_, DEVICE_ADDRESS, payload, sizeof(payload), nullptr, 0) == MSG_OK;
  i2cReleaseBus(i2c_driver_);
  return ok;
}

bool BQ2579::writeRegister16(uint8_t reg, uint16_t value) {
  const auto ptr = reinterpret_cast<uint8_t*>(&value);
  uint8_t payload[3] = {reg, ptr[0], ptr[1]};
  i2cAcquireBus(i2c_driver_);
  bool ok = i2cMasterTransmit(i2c_driver_, DEVICE_ADDRESS, payload, sizeof(payload), nullptr, 0) == MSG_OK;
  i2cReleaseBus(i2c_driver_);
  return ok;
}
