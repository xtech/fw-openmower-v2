#include "../include/sabo_robot.hpp"

#include <drivers/gps/nmea_gps_driver.h>
#include <ulog.h>

#include <drivers/adc/adc_driver.hpp>
#include <services.hpp>

using namespace xbot::driver::sabo;
using namespace xbot::driver::gps;
using namespace xbot::driver;

SaboRobot::SaboRobot() : hardware_config(GetHardwareConfig(GetHardwareVersion(carrier_board_info))) {
  // ADC configuration for input current measurement (channel 9 = PA4)
  static uint32_t adc_channels[] = {9};  // ADC12_IN9
  static adcsample_t adc_buffer[100];    // Buffer for 100 samples (single channel)

  xbot::driver::adc::AdcDriver::Config adc_config;
  adc_config.adc = &ADCD1;
  adc_config.channels = adc_channels;
  adc_config.num_channels = 1;
  adc_config.buffer = adc_buffer;
  adc_config.buffer_depth = 100;
  adc_config.sampling_time = ADC_SMPR_SMP_16P5;  // 16.5 cycles
  adc_config.circular = false;
  adc_config.vref = 3.3f;

  if (!adc_.Init(adc_config)) {
    ULOG_ERROR("SaboRobot: Failed to initialize ADC driver");
  }
}

void SaboRobot::InitPlatform() {
  InitMotors();
  charger_.setI2C(&I2CD1);
  bms_.Init();

  power_service.SetDriver(&charger_);
  power_service.SetDriver(&bms_);
  input_service.RegisterInputDriver("sabo", &sabo_input_driver_);

  cover_ui_.Start();

  // Initialize GPS driver for immediate availability (without waiting for ROS configuration)
  settings::GPSSettings gps_settings;
  if (!settings::GPSSettings::Load(gps_settings)) {
    ULOG_WARNING("Failed to load GPS settings from flash, using defaults");
  }
  gps_service.LoadAndStartGpsDriver(gps_settings.protocol, gps_settings.uart, gps_settings.baudrate);
}

bool SaboRobot::IsHardwareSupported() {
  // Accept Sabo 0.1|2|3.x boards
  if (strncmp("hw-openmower-sabo", carrier_board_info.board_id, sizeof(carrier_board_info.board_id)) == 0 &&
      strncmp("xcore", board_info.board_id, sizeof(board_info.board_id)) == 0 && board_info.version_major == 1 &&
      carrier_board_info.version_major == 0 &&
      (carrier_board_info.version_minor == 1 || carrier_board_info.version_minor == 2 ||
       carrier_board_info.version_minor == 3)) {
    return true;
  }

  return false;
}

bool SaboRobot::SaveGpsSettings(ProtocolType protocol, uint8_t uart, uint32_t baudrate) {
  settings::GPSSettings gps_settings;
  gps_settings.protocol = protocol;
  gps_settings.uart = uart;
  gps_settings.baudrate = baudrate;
  return settings::GPSSettings::Save(gps_settings);
}

float SaboRobot::Power_GetSystemCurrent() {
  // Start single conversion
  /*if (!adc_.StartConversion()) {
    ULOG_WARNING("SaboRobot: Failed to start ADC conversion");
    return std::numeric_limits<float>::quiet_NaN();
  }

  // Wait for conversion to complete (timeout 100ms)
  if (!adc_.WaitConversion(TIME_MS2I(100))) {
    adc_.StopConversion();
    ULOG_WARNING("SaboRobot: ADC conversion timeout");
    return std::numeric_limits<float>::quiet_NaN();
  }

  // Get voltage from channel 0 (input current sensor)
  float voltage = adc_.GetChannelVoltage(0);
  // Debug: get raw sample
  adcsample_t raw = adc_.GetChannelSample(0);

  // Convert voltage to current using INA180A1 constants
  using namespace xbot::driver::sabo::defs;
  float current = voltage / (ICS_VOLTAGE_DIVIDER_RATIO * ICS_SHUNT_RESISTANCE * ICS_GAIN);

  // Debug log
  ULOG_INFO("SaboRobot: ADC raw=%u, voltage=%.3fV, current=%.3fA", raw, voltage, current);

  return current;
  */
  return std::numeric_limits<float>::quiet_NaN();
}
