#include "../include/yardforce_robot.hpp"

#include <etl/array_view.h>
#include <hal.h>

#include <cmath>
#include <globals.hpp>
#include <services.hpp>

void YardForceRobot::InitPlatform() {
  InitMotors();
  charger_.setI2C(&I2CD1);
  power_service.SetDriver(&charger_);
  RegisterAdcSensors();

#if !STM32_UART_USE_UART7
#error STM32_UART_USE_UART7 must be enabled for the YF Cover UI to work
#endif
  input_service.RegisterInputDriver("yf_cover_ui", &yf_cover_ui_);
  yf_cover_ui_.Start(&UARTD7);

  // Enable aux power supply
  palSetLineMode(LINE_GPIO4, PAL_MODE_OUTPUT_PUSHPULL);
  palSetLine(LINE_GPIO4);
}

bool YardForceRobot::IsHardwareSupported() {
  // Accept YardForce 1.x.x boards
  if (strncmp("hw-openmower-yardforce", carrier_board_info.board_id, sizeof(carrier_board_info.board_id)) == 0 &&
      carrier_board_info.version_major == 1) {
    return true;
  }

  // Accept early testing boards
  if (strncmp("hw-xbot-devkit", carrier_board_info.board_id, sizeof(carrier_board_info.board_id)) == 0) {
    return true;
  }

  return false;
}

void YardForceRobot::RegisterAdcSensors() {
  using namespace xbot::driver::adc1;

  // ADC is only useable on carrier board v1.2.0 and later
  if (carrier_board_info.version_major < 1 ||
      (carrier_board_info.version_major == 1 && carrier_board_info.version_minor < 2)) {
    return;
  }

  Init();
  Start();

  // V-Charge sensor (ADC_CHANNEL_IN15)
  static const Adc1Sensor v_charge_sensors[] = {{.channel = ADC_CHANNEL_IN15, .sample_rate = ADC_SMPR_SMP_16P5}};
  static adcsample_t v_charge_buffer[sizeof(v_charge_sensors) / sizeof(v_charge_sensors[0])];
  static const Adc1ConversionGroup v_charge_cg = Adc1ConversionGroup::Create(
      Adc1ConversionId::V_CHARGER, etl::array_view<const Adc1Sensor>(v_charge_sensors), v_charge_buffer,
      Resolution::BITS_16, 0, 0,
      [](const Adc1ConversionGroup* self, float vref_voltage) -> float {
        if (self->user_data == nullptr) {
          return std::numeric_limits<float>::quiet_NaN();
        }
        const float scale_factor = *static_cast<const float*>(self->user_data);
        const auto& info = GetResolutionInfo(self->resolution);
        float voltage = info.RawToVoltage(self->sample_buffer[0], vref_voltage);
        return voltage * scale_factor;
      },
      const_cast<float*>(&ADC_CHARGER_VOLTAGE_SCALE), EmaFilterConfig{0.2f, 0.5f, 0.1f, true});
  RegisterConversionGroup(v_charge_cg);

  // V-Battery sensor (ADC_CHANNEL_IN16)
  static const Adc1Sensor v_battery_sensors[] = {{.channel = ADC_CHANNEL_IN16, .sample_rate = ADC_SMPR_SMP_16P5}};
  static adcsample_t v_battery_buffer[sizeof(v_battery_sensors) / sizeof(v_battery_sensors[0])];
  static const Adc1ConversionGroup v_battery_cg = Adc1ConversionGroup::Create(
      Adc1ConversionId::V_BATTERY, etl::array_view<const Adc1Sensor>(v_battery_sensors), v_battery_buffer,
      Resolution::BITS_16, 0, 0,
      [](const Adc1ConversionGroup* self, float vref_voltage) -> float {
        if (self->user_data == nullptr) {
          return std::numeric_limits<float>::quiet_NaN();
        }
        const float scale_factor = *static_cast<const float*>(self->user_data);
        const auto& info = GetResolutionInfo(self->resolution);
        float voltage = info.RawToVoltage(self->sample_buffer[0], vref_voltage);
        return voltage * scale_factor;
      },
      const_cast<float*>(&ADC_BATTERY_VOLTAGE_SCALE), EmaFilterConfig{0.3f, 0.7f, 0.5f, true});
  RegisterConversionGroup(v_battery_cg);
}
