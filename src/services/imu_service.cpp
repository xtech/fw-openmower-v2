//
// Created by clemens on 31.07.24.
//

#include "imu_service.hpp"

static int16_t data_raw_acceleration[3];
static int16_t data_raw_angular_rate[3];
static int16_t data_raw_temperature;
static double axes[9]{};
static float temperature_degC;

bool ImuService::Configure() { return true; }
void ImuService::OnStart() {}
void ImuService::OnStop() {}
void ImuService::OnCreate() {}
void ImuService::tick() {
  axes[0]++;

  SendAxes(axes, 9);
}
