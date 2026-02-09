//
// Created by clemens on 03.04.25.
//

#include "bq_2579.hpp"
BQ2579::~BQ2579() = default;

bool BQ2579::setAdapterCurrent(float current_amps, bool overwrite_hardware_limit) {
  (void)overwrite_hardware_limit;
  if (current_amps > 3.3f) {
    current_amps = 3.3f;
  }
  return writeRegister16(REG_Adapter_Current_Limit, static_cast<uint16_t>(current_amps * 100.0f));
}
bool BQ2579::setChargingCurrent(float current_amps, bool overwrite_hardware_limit) {
  (void)overwrite_hardware_limit;
  if (current_amps > 5.0f) {
    current_amps = 5.0f;
  }
  return writeRegister16(REG_Charge_Current_Limit, static_cast<uint16_t>(current_amps * 100.0f));
}
bool BQ2579::setPreChargeCurrent(float current_amps) {
  (void)current_amps;
  return true;
}
bool BQ2579::setTerminationCurrent(float current_amps) {
  if (current_amps > 1.0f) {
    current_amps = 1.0f;
  } else if (current_amps < 0.04f) {
    current_amps = 0.04f;
  }
  uint8_t raw_value = static_cast<uint8_t>(current_amps / 0.04f);
  return writeRegister8(REG_Termination_Control, raw_value);
}
CHARGER_STATUS BQ2579::getChargerStatus() {
  uint8_t fault0, fault1;
  if (!readRegister(REG_FAULT_Status_0, fault0)) {
    return CHARGER_STATUS::COMMS_ERROR;
  }
  if (!readRegister(REG_FAULT_Status_1, fault1)) {
    return CHARGER_STATUS::COMMS_ERROR;
  }

  if (fault0 || fault1) {
    return CHARGER_STATUS::FAULT;
  }

  uint8_t status;
  if (!readRegister(REG_Charger_Status_1, status)) {
    return CHARGER_STATUS::COMMS_ERROR;
  }

  switch (status >> 5) {
    case 0x00: return CHARGER_STATUS::NOT_CHARGING;
    case 0x01: return CHARGER_STATUS::TRICKLE;
    case 0x02: return CHARGER_STATUS::PRE_CHARGE;
    case 0x03: return CHARGER_STATUS::CC;
    case 0x04: return CHARGER_STATUS::CV;
    case 0x07: return CHARGER_STATUS::DONE;
    default: return CHARGER_STATUS::UNKNOWN;
  }
}
bool BQ2579::init() {
  // reset to default values
  if (!writeRegister8(REG_Termination_Control, 0b01000101)) {
    return false;
  }

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
bool BQ2579::readAdapterCurrent(float& result) {
  uint16_t raw_result = 0;
  if (!readRegister(REG_IBUS_ADC, raw_result)) return false;
  result = static_cast<float>(static_cast<int16_t>(raw_result)) / 1000.0f;
  return true;
}
bool BQ2579::readChargeCurrent(float& result) {
  uint16_t raw_result = 0;
  if (!readRegister(REG_IBAT_ADC, raw_result)) return false;
  result = static_cast<float>(static_cast<int16_t>(raw_result)) / 1000.0f;
  return true;
}
bool BQ2579::readAdapterVoltage(float& result) {
  uint16_t raw_result = 0;
  if (!readRegister(REG_VBUS_ADC, raw_result)) return false;
  result = static_cast<float>(raw_result) / 1000.0f;
  return true;
}
bool BQ2579::readBatteryVoltage(float& result) {
  uint16_t raw_result = 0;
  if (!readRegister(REG_VBAT_ADC, raw_result)) return false;
  result = static_cast<float>(raw_result) / 1000.0f;
  return true;
}

bool BQ2579::readSystemVoltage(float& result) {
  uint16_t raw_result = 0;
  if (!readRegister(REG_VSYS_ADC, raw_result)) return false;
  result = static_cast<float>(raw_result) / 1000.0f;
  return true;
}

bool BQ2579::readRegister(uint8_t reg, uint8_t& result) {
  if (i2c_driver_ == nullptr) {
    return false;
  }
  i2cAcquireBus(i2c_driver_);
  bool ok = i2cMasterTransmit(i2c_driver_, DEVICE_ADDRESS, &reg, sizeof(reg), &result, sizeof(result)) == MSG_OK;
  i2cReleaseBus(i2c_driver_);
  return ok;
}
bool BQ2579::readRegister(uint8_t reg, uint16_t& result) {
  if (i2c_driver_ == nullptr) {
    return false;
  }
  i2cAcquireBus(i2c_driver_);
  uint8_t buf[2];
  bool ok = i2cMasterTransmit(i2c_driver_, DEVICE_ADDRESS, &reg, sizeof(reg), buf, sizeof(buf)) == MSG_OK;
  result = buf[0] << 8 | buf[1];
  i2cReleaseBus(i2c_driver_);
  return ok;
}
bool BQ2579::writeRegister8(uint8_t reg, uint8_t value) {
  if (i2c_driver_ == nullptr) {
    return false;
  }
  uint8_t payload[2] = {reg, value};
  i2cAcquireBus(i2c_driver_);
  bool ok = i2cMasterTransmit(i2c_driver_, DEVICE_ADDRESS, payload, sizeof(payload), nullptr, 0) == MSG_OK;
  i2cReleaseBus(i2c_driver_);
  return ok;
}

bool BQ2579::writeRegister16(uint8_t reg, uint16_t value) {
  if (i2c_driver_ == nullptr) {
    return false;
  }
  const auto ptr = reinterpret_cast<uint8_t*>(&value);
  uint8_t payload[3] = {reg, ptr[1], ptr[0]};
  i2cAcquireBus(i2c_driver_);
  bool ok = i2cMasterTransmit(i2c_driver_, DEVICE_ADDRESS, payload, sizeof(payload), nullptr, 0) == MSG_OK;
  i2cReleaseBus(i2c_driver_);
  return ok;
}
