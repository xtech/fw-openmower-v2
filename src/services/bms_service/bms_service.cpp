/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "bms_service.hpp"

#include <cstring>

void BmsService::SetDriver(BmsDriver* bms_driver) {
  bms_ = bms_driver;
}

void BmsService::service_tick_() {
  if (bms_data_ == nullptr) return;

  StartTransaction();
  SendVoltage(bms_data_->pack_voltage_v);
  SendCurrent(bms_data_->pack_current_a);
  SendRelativeStateOfCharge(bms_data_->battery_soc);
  SendRemainingCapacity(bms_data_->remaining_capacity_ah);
  SendFullChargeCapacity(bms_data_->full_charge_capacity_ah);
  SendCycleCount(bms_data_->cycle_count);
  SendTemperature(bms_data_->temperature_c);
  SendBatteryStatus(bms_data_->battery_status);
  if (bms_extra_data_ != nullptr) {
    SendExtraData(bms_extra_data_, (uint32_t)strlen(bms_extra_data_));
  }
  CommitTransaction();
}

void BmsService::driver_tick_() {
  if (bms_ == nullptr) return;
  bms_->Tick();
  if (bms_->IsPresent()) {
    bms_data_ = bms_->GetData();
    bms_extra_data_ = bms_->GetExtraDataJson();
  }
}
