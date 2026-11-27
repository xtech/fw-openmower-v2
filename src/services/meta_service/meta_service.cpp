#include "meta_service.hpp"

#include <git_version.h>

#include <cstring>
#include <xbot-service/Io.hpp>

void MetaService::RPCGetFirmwareVersion(uint16_t call_id, char* data, uint16_t* response_length) {
  size_t len = strlen(BUILD_VERSION);
  if (len > *response_length) len = *response_length;
  memcpy(data, BUILD_VERSION, len);
  *response_length = static_cast<uint16_t>(len);
  SendRpcResponse(call_id, xbot::datatypes::RpcStatus::SUCCESS, data, len * sizeof(char));
}

void MetaService::RPCGetMajorVersion(uint16_t call_id) {
  uint16_t v = kFirmwareMajorVersion;
  SendRpcResponse(call_id, xbot::datatypes::RpcStatus::SUCCESS, &v, sizeof(v));
}

void MetaService::RPCGetRobotVariant(uint16_t call_id, char* data, uint16_t* response_length) {
  // Until main publishes the selected variant, BUSY means "not done, ask again" -
  // the high level retries instead of caching a wrong name.
  const char* variant = robot_variant_.load(std::memory_order_acquire);
  if (variant == nullptr) {
    SendRpcResponse(call_id, xbot::datatypes::RpcStatus::BUSY, nullptr, 0);
    return;
  }

  size_t len = strlen(variant);
  if (len > *response_length) len = *response_length;
  memcpy(data, variant, len);
  *response_length = static_cast<uint16_t>(len);
  SendRpcResponse(call_id, xbot::datatypes::RpcStatus::SUCCESS, data, len * sizeof(char));
}
