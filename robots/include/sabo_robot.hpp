#ifndef SABO_ROBOT_HPP
#define SABO_ROBOT_HPP

#include <drivers/charger/bq_2576/bq_2576.hpp>
#include <drivers/input/sabo_input_driver.hpp>
#include <drivers/ui/SaboCoverUI/sabo_cover_ui_controller.hpp>
#include <globals.hpp>

#include "robot.hpp"
#include "sabo_common.hpp"

using namespace xbot::driver::ui;
using namespace xbot::driver::motor;
using namespace xbot::driver::sabo;

class SaboRobot : public MowerRobot {
 public:
  // Hardware configuration, detected and initialized in constructor initializer list
  const config::HardwareConfig hardware_config;

  SaboRobot();

  void InitPlatform() override;
  bool IsHardwareSupported() override;

  UARTDriver* GPS_GetUartPort() override {
#ifndef STM32_UART_USE_USART6
#error STM32_SERIAL_USE_UART6 must be enabled for the Sabo build to work
#endif
    return &UARTD6;
  }

  float Power_GetDefaultBatteryFullVoltage() override {
    return 29.4f;  // As rated on battery pack, which is 7 * 4.2V
  }

  float Power_GetDefaultBatteryEmptyVoltage() override {
    return 7.0f * 3.3f;
  }

  float Power_GetDefaultChargeCurrent() override {
    // Battery pack is 7S3P, so max. would be 1.3Ah * 3 = 3.9A
    // 3.9A would be also the max. charge current for the stock PSU!
    return 1.95f;  // Lets stay save and conservative for now
  }

  float Power_GetAbsoluteMinVoltage() override {
    // Stock Sabo battery pack has INR18650-13L (Samsung) which are specified as:
    // Empty = 3.0V, Critical discharge <=2.5V. For now, let's stay save and use 3.0V
    return 7.0f * 3.0;
  }

  // ----- Some driver Test* functions used by Boot-Screen -----

  bool TestCharger() {
    const CHARGER_STATUS status = charger_.getChargerStatus();
    return status != CHARGER_STATUS::FAULT && status != CHARGER_STATUS::COMMS_ERROR;
  }

  template <typename EscDriver>
  bool TestESC(EscDriver& motor_driver) {
    if (!motor_driver.IsStarted()) return false;
    if (motor_driver.GetLatestState().status == MotorDriver::ESCState::ESCStatus::ESC_STATUS_DISCONNECTED) {
      motor_driver.RequestStatus();
      chThdSleepMilliseconds(100);  // give it a chance to respond
    }
    return motor_driver.GetLatestState().status == MotorDriver::ESCState::ESCStatus::ESC_STATUS_OK;
  }
  bool TestLeftESC() {
    return TestESC(left_motor_driver_);
  }
  bool TestRightESC() {
    return TestESC(right_motor_driver_);
  }
  bool TestMowerESC() {
    return TestESC(mower_motor_driver_);
  }

  // CoverUI button access for SaboInputDriver
  bool IsButtonPressed(const ButtonId button) const {
    return cover_ui_.IsButtonPressed(button);
  }
  uint16_t GetCoverUIButtonsMask() const {
    return cover_ui_.GetButtonsMask();
  }

  // GPIO sensor access for UI and other components
  bool GetSensorState(types::SensorId sensor_id) {
    return sabo_input_driver_.GetSensorState(sensor_id);
  }

  uint16_t GetSensorHeartbeatFrequency() const {
    return sabo_input_driver_.GetHeartbeatFrequency();
  }

  /**
   * @brief Enable/disable button blocking for InputScreen testing
   * @param block true to block all buttons, false to allow normal operation
   */
  void SetBlockButtons(bool block) {
    sabo_input_driver_.SetBlockButtons(block);
  }

 private:
  BQ2576 charger_{};
  SaboCoverUIController cover_ui_;
  SaboInputDriver sabo_input_driver_{};
};

#endif  // SABO_ROBOT_HPP
