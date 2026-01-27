#include "../include/sabo_robot.hpp"

#include <drivers/gps/nmea_gps_driver.h>
#include <ulog.h>

#include <drivers/adc/adc_driver.hpp>
#include <services.hpp>

using namespace xbot::driver::sabo;
using namespace xbot::driver::gps;
using namespace xbot::driver;
using namespace xbot::driver::adc;

SaboRobot::SaboRobot() : hardware_config(GetHardwareConfig(GetHardwareVersion(carrier_board_info))) {
}

void SaboRobot::InitPlatform() {
  InitMotors();
  charger_.setI2C(&I2CD1);
  bms_.Init();

  power_service.SetDriver(&charger_);
  power_service.SetDriver(&bms_);
  input_service.RegisterInputDriver("sabo", &sabo_input_driver_);

  if (hardware_config.adc != nullptr) {
    InitAdcDriver(CreateAdcConfig());
  }

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

AdcConfig SaboRobot::CreateAdcConfig() {
  static const xbot::driver::sabo::config::Adc* adc_scales = hardware_config.adc;

  // Channels
  static const AdcChannel channels[] = {AdcChannel::Create(
                                            ChannelId::V_CHARGER, ADC_CHANNEL_IN15, ADC_SMPR_SMP_810P5,
                                            [](adcsample_t raw, const void* user_data) -> float {
                                              float voltage = AdcChannel::RawToVoltage(raw);
                                              float factor = user_data ? *static_cast<const float*>(user_data) : 1.0f;
                                              return voltage * factor;
                                            },
                                            &adc_scales->charger_voltage_scale_factor),
                                        AdcChannel::Create(
                                            ChannelId::V_BATTERY, ADC_CHANNEL_IN16, ADC_SMPR_SMP_810P5,
                                            [](adcsample_t raw, const void* user_data) -> float {
                                              float voltage = AdcChannel::RawToVoltage(raw);
                                              float factor = user_data ? *static_cast<const float*>(user_data) : 1.0f;
                                              return voltage * factor;
                                            },
                                            &adc_scales->battery_voltage_scale_factor),
                                        AdcChannel::Create(
                                            ChannelId::I_IN_DCDC, ADC_CHANNEL_IN18, ADC_SMPR_SMP_810P5,
                                            [](adcsample_t raw, const void* user_data) -> float {
                                              float voltage = AdcChannel::RawToVoltage(raw);
                                              float factor = user_data ? *static_cast<const float*>(user_data) : 1.0f;
                                              return voltage * factor;
                                            },
                                            &adc_scales->dcdc_in_current_scale_factor)};

  // ADC
  size_t const num_channels = sizeof(channels) / sizeof(channels[0]);
  static adcsample_t sample_buffer[num_channels];
  static const AdcConfig config = {.drv = &ADCD1,
                                   .channels = etl::array_view<const AdcChannel>(channels, num_channels),
                                   .sample_buffer = sample_buffer};

  return config;
}
