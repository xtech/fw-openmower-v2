#include "robot.hpp"
#include <globals.hpp>
#include <drivers/charger/bq_2579/bq_2579.hpp>

namespace Robot {

static BQ2579 charger{};

namespace General {
void InitPlatform() {
  auto* pwm_config = new PWMConfig();
  auto* pwm_config2 = new PWMConfig();

  chDbgAssert(pwm_config != nullptr, "ERR_MEM");
  chDbgAssert(pwm_config2 != nullptr, "ERR_MEM");

  memset(pwm_config, 0, sizeof(PWMConfig));
  memset(pwm_config2, 0, sizeof(PWMConfig));

  palSetLineMode(LINE_MOTOR1_PWM1, LINE_MOTOR1_PWM1_MODE);
  palSetLineMode(LINE_MOTOR1_PWM2, LINE_MOTOR1_PWM2_MODE);
  palSetLineMode(LINE_MOTOR2_PWM1, LINE_MOTOR2_PWM1_MODE);
  palSetLineMode(LINE_MOTOR2_PWM2, LINE_MOTOR2_PWM2_MODE);


  pwm_config->channels[MOTOR1_PWM_CHANNEL_1].mode = PWM_OUTPUT_ACTIVE_LOW;
  pwm_config->channels[MOTOR1_PWM_CHANNEL_2].mode = PWM_OUTPUT_ACTIVE_LOW;
  pwm_config->period = 0xFFF * 4;
  pwm_config->frequency = 275000000;
  pwm_config2->channels[MOTOR2_PWM_CHANNEL_1].mode = PWM_OUTPUT_ACTIVE_LOW;
  pwm_config2->channels[MOTOR2_PWM_CHANNEL_2].mode = PWM_OUTPUT_ACTIVE_LOW;
  pwm_config2->period = 0xFFF * 4;
  pwm_config2->frequency = 275000000;



  pwmStart(&MOTOR1_PWM, pwm_config);
  if (&MOTOR1_PWM != &MOTOR2_PWM) {
    pwmStart(&MOTOR2_PWM, pwm_config2);
  }

  pwmEnableChannel(&MOTOR1_PWM, MOTOR1_PWM_CHANNEL_1, 0);
  pwmEnableChannel(&MOTOR1_PWM, MOTOR1_PWM_CHANNEL_2, 0);
  pwmEnableChannel(&MOTOR2_PWM, MOTOR2_PWM_CHANNEL_1, 0);
  pwmEnableChannel(&MOTOR2_PWM, MOTOR2_PWM_CHANNEL_2, 0);



  palSetLineMode(LINE_POWER_1_ENABLE, PAL_MODE_OUTPUT_PUSHPULL);
  palSetLineMode(LINE_POWER_2_ENABLE, PAL_MODE_OUTPUT_PUSHPULL);

  palSetLineMode(LINE_AUX_POWER_1_ENABLE, PAL_MODE_OUTPUT_PUSHPULL);
  palSetLineMode(LINE_AUX_POWER_1_STATUS, PAL_MODE_INPUT);
  palSetLineMode(LINE_AUX_POWER_2_ENABLE, PAL_MODE_OUTPUT_PUSHPULL);
  palSetLineMode(LINE_AUX_POWER_2_STATUS, PAL_MODE_INPUT);
  palSetLineMode(LINE_AUX_POWER_3_ENABLE, PAL_MODE_OUTPUT_PUSHPULL);
  palSetLineMode(LINE_AUX_POWER_3_STATUS, PAL_MODE_INPUT);


}

bool IsHardwareSupported() {
  // Accept YardForce 1.x.x boards
  return strncmp("hw-xbot-mainboard", carrier_board_info.board_id, sizeof(carrier_board_info.board_id)) == 0 &&
      carrier_board_info.version_major == 0;
}

}  // namespace General

namespace GPS {
UARTDriver* GetUartPort() {
  return nullptr;
}
}

namespace Power {

I2CDriver* GetPowerI2CD() {
  return &I2CD1;
}

Charger* GetCharger() {
  return &charger;
}

float GetMaxVoltage() {
  return 4.0f * 4.2f;
}

float GetChargeCurrent() {
  return 1.0;
}

float GetMinVoltage() {
  // 3.3V min, 4s pack
  return 4.0f * 3.3;
}

}  // namespace Power
}  // namespace Robot
