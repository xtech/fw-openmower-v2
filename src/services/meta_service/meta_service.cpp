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
