/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef BMS_SERVICE_HPP
#define BMS_SERVICE_HPP

#include <ch.h>

#include <BmsServiceBase.hpp>
#include <drivers/bms/bms_driver.hpp>

using namespace xbot::service;
using namespace xbot::driver::bms;

class BmsService : public BmsServiceBase {
 public:
  explicit BmsService(uint16_t service_id) : BmsServiceBase(service_id, wa, sizeof(wa)) {
  }

  void SetDriver(BmsDriver* bms_driver);

 private:
  void service_tick_();
  void driver_tick_();

  ServiceSchedule tick_schedule_{*this, 1'000'000,
                                 XBOT_FUNCTION_FOR_METHOD(BmsService, &BmsService::service_tick_, this)};
  Schedule driver_schedule_{scheduler_, true, 1'000'000,
                            XBOT_FUNCTION_FOR_METHOD(BmsService, &BmsService::driver_tick_, this)};

  BmsDriver* bms_ = nullptr;
  const Data* bms_data_ = nullptr;
  const char* bms_extra_data_ = nullptr;
  THD_WORKING_AREA(wa, 2048){};
};

#endif  // BMS_SERVICE_HPP
