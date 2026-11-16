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
  if (bms_live_ == nullptr) return;

  StartTransaction();
  SendVoltage(bms_live_->pack_voltage_v);
  SendCurrent(bms_live_->pack_current_a);
  SendRelativeStateOfCharge(bms_live_->battery_soc);
  SendRemainingCapacity(bms_live_->remaining_capacity_ah);
  SendFullChargeCapacity(bms_live_->full_charge_capacity_ah);
  SendCycleCount(bms_live_->cycle_count);
  SendTemperature(bms_live_->temperature_c);
  SendBatteryStatus(bms_live_->battery_status);
  SendBatteryStatusRaw(bms_live_->battery_status_raw);
  SendCellVoltages(bms_live_->cell_voltage_v, bms_info_->cell_count);
  CommitTransaction();
}

void BmsService::driver_tick_() {
  if (bms_ == nullptr) return;
  bms_->Tick();
  if (bms_->IsPresent()) {
    bms_info_ = &bms_->GetDeviceInfo();
    bms_live_ = &bms_->GetLiveData();
  } else {
    // Do not keep sending the last values as if they were live.
    bms_info_ = nullptr;
    bms_live_ = nullptr;
  }
}

void BmsService::RPCGetDeviceInfo(uint16_t call_id, char* data, uint16_t* response_length) {
  // The dispatcher hands in the capacity of the return value and validates it against the
  // schema's max_length, so the driver can build the JSON straight into it.
  const size_t written = (bms_ != nullptr && bms_->IsPresent()) ? bms_->BuildDeviceInfo(data, *response_length) : 0U;
  *response_length = static_cast<uint16_t>(written);
  SendRpcResponse(call_id, xbot::datatypes::RpcStatus::SUCCESS, data, written);
}
