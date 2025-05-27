#include "xbot_robot.hpp"

#include <services.hpp>

#define LINE_MOTOR1_PWM1 LINE_UART1_RX
#define LINE_MOTOR1_PWM1_MODE PAL_MODE_ALTERNATE(1)
#define LINE_MOTOR1_PWM2 LINE_UART1_TX
#define LINE_MOTOR1_PWM2_MODE PAL_MODE_ALTERNATE(1)
#define LINE_MOTOR1_ENCODER_A LINE_GPIO16
#define LINE_MOTOR1_ENCODER_B LINE_GPIO17
#define MOTOR1_PWM PWMD1
#define MOTOR1_PWM_CHANNEL_1 1
#define MOTOR1_PWM_CHANNEL_2 2

#define LINE_MOTOR2_PWM1 LINE_UART6_RX
#define LINE_MOTOR2_PWM1_MODE PAL_MODE_ALTERNATE(2)
#define LINE_MOTOR2_PWM2 LINE_UART6_TX
#define LINE_MOTOR2_PWM2_MODE PAL_MODE_ALTERNATE(2)
#define LINE_MOTOR2_ENCODER_A LINE_GPIO18
#define LINE_MOTOR2_ENCODER_B LINE_GPIO19
#define MOTOR2_PWM PWMD3
#define MOTOR2_PWM_CHANNEL_1 0
#define MOTOR2_PWM_CHANNEL_2 1

#define LINE_AUX_POWER_1_ENABLE LINE_GPIO10
#define LINE_AUX_POWER_1_STATUS LINE_GPIO11
#define LINE_AUX_POWER_2_ENABLE LINE_GPIO12
#define LINE_AUX_POWER_2_STATUS LINE_GPIO13
#define LINE_AUX_POWER_3_ENABLE LINE_GPIO14
#define LINE_AUX_POWER_3_STATUS LINE_GPIO15
#define LINE_POWER_1_ENABLE LINE_GPIO8
#define LINE_POWER_2_ENABLE LINE_GPIO9

#define LINE_CHARGER_ENABLE LINE_GPIO6

#define NO_MOWER_SERVICE

void xBotRobot::InitPlatform() {
  // Configure PWM and setup motor drivers
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

  left_pwm_motor_driver_.SetPWM(&MOTOR1_PWM, MOTOR1_PWM_CHANNEL_1, MOTOR1_PWM_CHANNEL_2);
  left_pwm_motor_driver_.SetEncoder(LINE_MOTOR1_ENCODER_A, LINE_MOTOR1_ENCODER_B);
  right_pwm_motor_driver_.SetPWM(&MOTOR2_PWM, MOTOR2_PWM_CHANNEL_1, MOTOR2_PWM_CHANNEL_2);
  right_pwm_motor_driver_.SetEncoder(LINE_MOTOR2_ENCODER_A, LINE_MOTOR2_ENCODER_B);
  diff_drive.SetDrivers(&left_pwm_motor_driver_, &right_pwm_motor_driver_);

  palSetLineMode(LINE_POWER_1_ENABLE, PAL_MODE_OUTPUT_PUSHPULL);
  palSetLineMode(LINE_POWER_2_ENABLE, PAL_MODE_OUTPUT_PUSHPULL);

  palSetLineMode(LINE_AUX_POWER_1_ENABLE, PAL_MODE_OUTPUT_PUSHPULL);
  palSetLineMode(LINE_AUX_POWER_1_STATUS, PAL_MODE_INPUT);
  palSetLineMode(LINE_AUX_POWER_2_ENABLE, PAL_MODE_OUTPUT_PUSHPULL);
  palSetLineMode(LINE_AUX_POWER_2_STATUS, PAL_MODE_INPUT);
  palSetLineMode(LINE_AUX_POWER_3_ENABLE, PAL_MODE_OUTPUT_PUSHPULL);
  palSetLineMode(LINE_AUX_POWER_3_STATUS, PAL_MODE_INPUT);

  charger_.setI2C(&I2CD1);
  power_service.SetDriver(&charger_);
}

bool xBotRobot::IsHardwareSupported() {
  // Accept YardForce 1.x.x boards
  return strncmp("hw-xbot-mainboard", carrier_board_info.board_id, sizeof(carrier_board_info.board_id)) == 0 &&
         carrier_board_info.version_major == 0;
}
