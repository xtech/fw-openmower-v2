#include "../include/sabo_robot.hpp"

#include <drivers/gps/nmea_gps_driver.h>
#include <etl/delegate.h>
#include <ulog.h>

#include <services.hpp>

using namespace xbot::driver::sabo;
using namespace xbot::driver::gps;
using namespace xbot::driver;

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
    adc1::Init();
    adc1::Start();
    RegisterAdc1Sensors();
  }

  power_service.SetPowerManagementCallback(
      etl::delegate<void()>::create<SaboRobot, &SaboRobot::OnPowerManagement>(*this));

  // Initialize GPS driver for immediate availability (without waiting for ROS configuration)
  settings::GPSSettings gps_settings;
  if (!settings::GPSSettings::Load(gps_settings)) {
    ULOG_WARNING("Failed to load GPS settings from flash, using defaults");
  }
  gps_service.LoadAndStartGpsDriver(gps_settings.protocol, gps_settings.uart, gps_settings.baudrate);

  cover_ui_.Start();
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

void SaboRobot::RegisterAdc1Sensors() {
  static const xbot::driver::sabo::config::Adc* adc_scales = hardware_config.adc;

  // V-Charge sensor
  static const Adc1Sensor v_charge_sensors[] = {{.channel = ADC_CHANNEL_IN15, .sample_rate = ADC_SMPR_SMP_16P5}};
  static adcsample_t v_charge_buffer[sizeof(v_charge_sensors) / sizeof(v_charge_sensors[0])];
  // Create ADC conversion group and place sensor(s)
  static const Adc1ConversionGroup v_charge_cg = Adc1ConversionGroup::Create(
      Adc1ConversionId::V_CHARGER,                          // Conversion ID
      etl::array_view<const Adc1Sensor>(v_charge_sensors),  // Sensor(s)/channel definition
      v_charge_buffer,                                      // ADC sampple/data buffer
      Resolution::BITS_16,                                  // ADC resolution
      0, 0,                                                 // Oversample ratio & right-shift
      // ADC raw data to final value convert function (lambda)
      [](const Adc1ConversionGroup* self, float vref_voltage) -> float {
        if (self->user_data == nullptr) {
          return std::numeric_limits<float>::quiet_NaN();
        }
        const float scale_factor = *static_cast<const float*>(self->user_data);
        const auto& info = GetResolutionInfo(self->resolution);
        float voltage = info.RawToVoltage(self->sample_buffer[0], vref_voltage);
        return voltage * scale_factor;
      },
      &adc_scales->charger_voltage_scale_factor);  // User data
  adc1::RegisterConversionGroup(v_charge_cg);

  // V-Battery sensor
  static const Adc1Sensor v_battery_sensors[] = {{.channel = ADC_CHANNEL_IN16, .sample_rate = ADC_SMPR_SMP_16P5}};
  static adcsample_t v_battery_buffer[sizeof(v_battery_sensors) / sizeof(v_battery_sensors[0])];
  // Create ADC conversion group and place sensor(s)
  static const Adc1ConversionGroup v_battery_cg = Adc1ConversionGroup::Create(
      Adc1ConversionId::V_BATTERY,                           // Conversion ID
      etl::array_view<const Adc1Sensor>(v_battery_sensors),  // Sensor(s)/channel definition
      v_battery_buffer,                                      // ADC sampple/data buffer
      Resolution::BITS_16,                                   // ADC resolution
      0, 0,                                                  // Oversample ratio & right-shift
      // ADC raw data to final value convert function (lambda)
      [](const Adc1ConversionGroup* self, float vref_voltage) -> float {
        if (self->user_data == nullptr) {
          return std::numeric_limits<float>::quiet_NaN();
        }
        (void)vref_voltage;
        const float scale_factor = *static_cast<const float*>(self->user_data);
        const auto& info = GetResolutionInfo(self->resolution);
        float voltage = info.RawToVoltage(self->sample_buffer[0], vref_voltage);
        return voltage * scale_factor;
      },
      &adc_scales->battery_voltage_scale_factor);  // User data
  adc1::RegisterConversionGroup(v_battery_cg);

  // I-IN-DCDC sensor
  static const Adc1Sensor i_dcdc_sensors[] = {{.channel = ADC_CHANNEL_IN18, .sample_rate = ADC_SMPR_SMP_8P5}};
  static adcsample_t i_dcdc_buffer[sizeof(i_dcdc_sensors) / sizeof(i_dcdc_sensors[0])];
  // Create ADC conversion group and place sensor(s)
  static const Adc1ConversionGroup i_dcdc_cg = Adc1ConversionGroup::Create(
      Adc1ConversionId::I_IN_DCDC,                        // Conversion ID
      etl::array_view<const Adc1Sensor>(i_dcdc_sensors),  // Sensor(s)/channel definition
      i_dcdc_buffer,                                      // ADC sampple/data buffer
      Resolution::BITS_16,                                // ADC resolution
      16, 4,                                              // Oversample ratio & right-shift
      // ADC raw data to final value convert function (lambda)
      [](const Adc1ConversionGroup* self, float vref_voltage) -> float {
        if (self->user_data == nullptr) {
          return std::numeric_limits<float>::quiet_NaN();
        }
        const float scale_factor = *static_cast<const float*>(self->user_data);
        const auto& info = GetResolutionInfo(self->resolution);
        float voltage = info.RawToVoltage(self->sample_buffer[0], vref_voltage);
        return voltage * scale_factor;
      },
      &adc_scales->dcdc_in_current_scale_factor);  // User data
  adc1::RegisterConversionGroup(i_dcdc_cg);
}

void SaboRobot::OnPowerManagement() {
  static float last_adapter_limit = 0.0f;
  static float last_charge_limit = 0.0f;

  constexpr float HYSTERESIS = 0.05f;          // 50mA
  constexpr float SAFETY_RESERVE = 0.2f;       // 200mA (~100mA blind current into ESCs + 100mA ADC inaccuracy)
  constexpr float MIN_ADAPTER_CURRENT = 0.3f;  // 300mA
  constexpr float MAX_ADAPTER_CURRENT = 4.5f;  // Hardware limit: PCB traces = ~4.9A = 4.5A conservative
  constexpr float MAX_CHARGE_CURRENT = 5.5f;   // Hardware limit: PCB traces = ~5.9A = 5.5A conservative

  float system_current = power_service.GetConfiguredSystemCurrent();
  if (isnan(system_current) || system_current <= 0.0f) return;

  float dcdc_current = power_service.GetDCDCCurrent();
  if (isnan(dcdc_current) || dcdc_current <= 0.0f) return;

  // Clamp adapter current to a minimum of MIN_ADAPTER_CURRENT
  float adapter_limit = std::max(MIN_ADAPTER_CURRENT, system_current - dcdc_current - SAFETY_RESERVE);
  adapter_limit = std::min(adapter_limit, MAX_ADAPTER_CURRENT);  // Clamp to <= MAX_ADAPTER_CURRENT
  if (fabsf(adapter_limit - last_adapter_limit) > HYSTERESIS) {  // Don't flood charger with minor corrections
    charger_.setAdapterCurrent(adapter_limit);
    last_adapter_limit = adapter_limit;
  }

  // Ensure charge current is within Sabo's design limits before disabling ICHG pin
  float config_charge_current = power_service.GetConfiguredChargeCurrent();
  float charge_limit = (!isnan(config_charge_current) && config_charge_current > 0.0f)
                           ? config_charge_current
                           : Power_GetDefaultChargeCurrent();  // Sabo default
  charge_limit = std::min(MAX_CHARGE_CURRENT, charge_limit);   // Clamp to <= MAX_CHARGE_CURRENT
  if (last_charge_limit != charge_limit) {
    charger_.setChargingCurrent(charge_limit, true);
    last_charge_limit = charge_limit;
  }
}
