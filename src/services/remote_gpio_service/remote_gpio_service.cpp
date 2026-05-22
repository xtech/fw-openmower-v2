#include "remote_gpio_service.hpp"

#include <etl/algorithm.h>
#include <ulog.h>

#include <board_utils.hpp>
#include <globals.hpp>
#include <json_stream.hpp>

using xbot::datatypes::RpcStatus;
using xbot::service::HeatshrinkDataSource;

extern RemoteGPIOService remote_gpio_service;

// ─── GPIO interrupt callback (ISR-safe) ─────────────────────────────────────

static void RemoteGpioLineCallback(void*) {
  remote_gpio_service.SendEvent(Events::REMOTE_GPIO_TRIGGERED);
}

// ─── Config JSON parsing state ───────────────────────────────────────────────

struct RemoteGpioConfigJsonData : public json_data_t {
  enum class Section { NONE, GPIOS, I2C } section = Section::NONE;
  bool in_entry = false;
  // Temp GPIO entry
  uint8_t pin_id = 0;
  ioline_t pin_line = PAL_NOLINE;
  bool pin_is_output = false;
  bool pin_got_direction = false;
  uint8_t pin_default = 0;
  // Temp I2C entry
  uint8_t bus_id = 0;
  I2CDriver* bus_driver = nullptr;
};

static I2CDriver* BusNameToDriver(const char* name) {
  if (strcmp(name, "I2C1") == 0) return &I2CD1;
  if (strcmp(name, "I2C2") == 0) return &I2CD2;
  if (strcmp(name, "I2C4") == 0) return &I2CD4;
  return nullptr;
}

bool RemoteGPIOService::ConfigJsonCallback(lwjson_stream_parser_t* jsp, lwjson_stream_type_t type, void* data_voidptr) {
  auto* d = static_cast<RemoteGpioConfigJsonData*>(data_voidptr);

  switch (jsp->stack_pos) {
    case 0: JsonExpectTypeOrEnd(OBJECT); break;

    // Top-level key: "gpios" or "i2c". Unknown keys → NONE (skip, forward compat).
    case 1:
      if (type == LWJSON_STREAM_TYPE_KEY) {
        if (strcmp(jsp->data.str.buff, "gpios") == 0) {
          d->section = RemoteGpioConfigJsonData::Section::GPIOS;
        } else if (strcmp(jsp->data.str.buff, "i2c") == 0) {
          d->section = RemoteGpioConfigJsonData::Section::I2C;
        } else {
          d->section = RemoteGpioConfigJsonData::Section::NONE;
        }
      }
      break;

    // Value of a top-level key — must be an array for known sections.
    // Unknown sections (NONE) skip silently regardless of type.
    case 2:
      if (d->section == RemoteGpioConfigJsonData::Section::NONE) break;
      JsonExpectTypeOrEnd(ARRAY);
      break;

    // Entry object open/close. Unknown sections skip silently.
    case 3: {
      if (d->section == RemoteGpioConfigJsonData::Section::NONE) break;
      JsonExpectTypeOrEnd(OBJECT);
      if (type == LWJSON_STREAM_TYPE_OBJECT) {
        d->in_entry = true;
        d->pin_id = 0;
        d->pin_line = PAL_NOLINE;
        d->pin_is_output = false;
        d->pin_got_direction = false;
        d->pin_default = 0;
        d->bus_id = 0;
        d->bus_driver = nullptr;
      } else {
        // OBJECT_END: all field values at depth 5 already fired sequentially
        // before this point, so pin_line / bus_driver etc. are fully populated.
        if (d->section == RemoteGpioConfigJsonData::Section::GPIOS) {
          if (d->pin_line == PAL_NOLINE) {
            ULOG_ERROR("RemoteGPIO: GPIO entry missing \"line\"");
            return false;
          }
          if (!d->pin_got_direction) {
            ULOG_ERROR("RemoteGPIO: GPIO entry missing \"direction\"");
            return false;
          }
          if (gpios_.full()) {
            ULOG_ERROR("RemoteGPIO: Too many GPIOs (max %d)", kMaxGPIOs);
            return false;
          }
          auto& pin = gpios_.emplace_back();
          pin.id = d->pin_id;
          pin.line = d->pin_line;
          pin.is_output = d->pin_is_output;
          pin.default_value = d->pin_default;
          pin.subscribed = false;
          pin.periodic = false;
          pin.last_value = 0;
        } else if (d->section == RemoteGpioConfigJsonData::Section::I2C) {
          if (d->bus_driver == nullptr) {
            ULOG_ERROR("RemoteGPIO: I2C entry missing or unknown \"bus\"");
            return false;
          }
          if (i2c_buses_.full()) {
            ULOG_ERROR("RemoteGPIO: Too many I2C buses (max %d)", kMaxI2CBuses);
            return false;
          }
          auto& bus = i2c_buses_.emplace_back();
          bus.id = d->bus_id;
          bus.driver = d->bus_driver;
        }
        d->in_entry = false;
      }
      break;
    }

    // Key-value pairs inside entry (key at depth 4, value at depth 5)
    case 5: {
      if (!d->in_entry) break;
      const char* key = jsp->stack[4].meta.name;

      if (d->section == RemoteGpioConfigJsonData::Section::GPIOS) {
        if (strcmp(key, "id") == 0) {
          return JsonGetNumber(jsp, type, d->pin_id);
        } else if (strcmp(key, "line") == 0) {
          JsonExpectType(STRING);
          d->pin_line = GetIoLineByName(jsp->data.str.buff);
          if (d->pin_line == PAL_NOLINE) {
            ULOG_ERROR("RemoteGPIO: Unknown GPIO line \"%s\"", jsp->data.str.buff);
            return false;
          }
        } else if (strcmp(key, "direction") == 0) {
          JsonExpectType(STRING);
          if (strcmp(jsp->data.str.buff, "output") == 0) {
            d->pin_is_output = true;
          } else if (strcmp(jsp->data.str.buff, "input") == 0) {
            d->pin_is_output = false;
          } else {
            ULOG_ERROR("RemoteGPIO: Invalid direction \"%s\"", jsp->data.str.buff);
            return false;
          }
          d->pin_got_direction = true;
        } else if (strcmp(key, "default") == 0) {
          return JsonGetNumber(jsp, type, d->pin_default);
        } else if (strcmp(key, "name") == 0) {
          // informational only
        } else {
          ULOG_ERROR("RemoteGPIO: Unknown GPIO attribute \"%s\"", key);
          return false;
        }
      } else if (d->section == RemoteGpioConfigJsonData::Section::I2C) {
        if (strcmp(key, "id") == 0) {
          return JsonGetNumber(jsp, type, d->bus_id);
        } else if (strcmp(key, "bus") == 0) {
          JsonExpectType(STRING);
          d->bus_driver = BusNameToDriver(jsp->data.str.buff);
          if (d->bus_driver == nullptr) {
            ULOG_ERROR("RemoteGPIO: Unknown I2C bus \"%s\"", jsp->data.str.buff);
            return false;
          }
        } else if (strcmp(key, "name") == 0) {
          // informational only
        } else {
          ULOG_ERROR("RemoteGPIO: Unknown I2C attribute \"%s\"", key);
          return false;
        }
      }
      break;
    }
  }
  return true;
}

// ─── Hardware setup/teardown ─────────────────────────────────────────────────

void RemoteGPIOService::SetUpHardware() {
  for (auto& pin : gpios_) {
    if (pin.is_output) {
      palSetLineMode(pin.line, PAL_MODE_OUTPUT_PUSHPULL);
      palWriteLine(pin.line, pin.default_value ? PAL_HIGH : PAL_LOW);
      pin.last_value = pin.default_value;
    } else {
      palSetLineMode(pin.line, PAL_MODE_INPUT);
      palSetLineCallback(pin.line, RemoteGpioLineCallback, nullptr);
      palEnableLineEvent(pin.line, PAL_EVENT_MODE_BOTH_EDGES);
      pin.last_value = palReadLine(pin.line) == PAL_HIGH ? 1 : 0;
    }
  }
}

void RemoteGPIOService::TearDownHardware() {
  for (auto& pin : gpios_) {
    if (!pin.is_output) {
      palDisableLineEvent(pin.line);
    }
  }
}

void RemoteGPIOService::ClearSubscriptions() {
  for (auto& pin : gpios_) {
    pin.subscribed = false;
    pin.periodic = false;
  }
  last_periodic_us_ = 0;
}

// ─── Lifecycle ───────────────────────────────────────────────────────────────

bool RemoteGPIOService::OnRegisterGPIOConfigsChanged(const void* data, size_t length) {
  TearDownHardware();
  ClearSubscriptions();
  gpios_.clear();
  i2c_buses_.clear();

  HeatshrinkDataSource source{static_cast<const uint8_t*>(data), length};

  RemoteGpioConfigJsonData json_data;
  json_data.callback = etl::make_delegate<RemoteGPIOService, &RemoteGPIOService::ConfigJsonCallback>(*this);

  if (!ProcessJson(source, json_data)) {
    ULOG_ERROR("RemoteGPIO: Config parsing failed");
    gpios_.clear();
    i2c_buses_.clear();
    return false;
  }

  if (IsRunning()) {
    SetUpHardware();
  }
  return true;
}

bool RemoteGPIOService::OnStart() {
  SetUpHardware();
  return true;
}

void RemoteGPIOService::OnStop() {
  TearDownHardware();
  ClearSubscriptions();
}

// ─── Main loop ───────────────────────────────────────────────────────────────

uint32_t RemoteGPIOService::OnLoop(uint32_t now_micros, uint32_t) {
  // Check GPIO interrupt events
  eventmask_t events = chEvtGetAndClearEvents(Events::ids_to_mask({Events::REMOTE_GPIO_TRIGGERED}));
  if (events & EVENT_MASK(Events::REMOTE_GPIO_TRIGGERED)) {
    for (auto& pin : gpios_) {
      if (!pin.subscribed || pin.is_output) continue;
      uint8_t new_val = palReadLine(pin.line) == PAL_HIGH ? 1 : 0;
      if (new_val != pin.last_value) {
        EmitGpioEvent(pin.id, pin.last_value, new_val, 0u);
        pin.last_value = new_val;
      }
    }
  }

  // Periodic updates
  if (HasAnyPeriodicSubscription()) {
    uint32_t interval_us = PeriodicUpdateInterval.value * 1000u;
    if (now_micros - last_periodic_us_ >= interval_us) {
      last_periodic_us_ = now_micros;
      for (auto& pin : gpios_) {
        if (!pin.periodic) continue;
        uint8_t current_val = pin.is_output ? pin.last_value : (palReadLine(pin.line) == PAL_HIGH ? 1 : 0);
        EmitGpioEvent(pin.id, pin.last_value, current_val, static_cast<uint8_t>(GPIOEventFlags::PERIODIC));
        pin.last_value = current_val;
      }
    }
    uint32_t elapsed = now_micros - last_periodic_us_;
    return interval_us > elapsed ? interval_us - elapsed : 0u;
  }

  return UINT32_MAX;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

RemoteGPIOService::GpioPin* RemoteGPIOService::FindGpio(uint8_t id) {
  for (auto& pin : gpios_) {
    if (pin.id == id) return &pin;
  }
  return nullptr;
}

RemoteGPIOService::I2CBus* RemoteGPIOService::FindBus(uint8_t id) {
  for (auto& bus : i2c_buses_) {
    if (bus.id == id) return &bus;
  }
  return nullptr;
}

void RemoteGPIOService::EmitGpioEvent(uint8_t gpio_id, uint8_t old_val, uint8_t new_val, uint8_t flags) {
  uint8_t event[4] = {gpio_id, old_val, new_val, flags};
  SendGPIOEvent(event, 4);
}

bool RemoteGPIOService::HasAnyPeriodicSubscription() const {
  for (const auto& pin : gpios_) {
    if (pin.periodic) return true;
  }
  return false;
}

// ─── GPIO RPCs ───────────────────────────────────────────────────────────────

void RemoteGPIOService::RPCReadGPIO(uint16_t call_id, uint8_t GPIOID) {
  auto* pin = FindGpio(GPIOID);
  if (!pin) {
    SendRpcResponse(call_id, RpcStatus::ERROR, nullptr, 0);
    return;
  }
  uint8_t val = pin->is_output ? pin->last_value : (palReadLine(pin->line) == PAL_HIGH ? 1u : 0u);
  SendRpcResponse(call_id, RpcStatus::SUCCESS, &val, sizeof(val));
}

void RemoteGPIOService::RPCWriteGPIO(uint16_t call_id, uint8_t GPIOID, uint8_t Value) {
  auto* pin = FindGpio(GPIOID);
  if (!pin || !pin->is_output) {
    uint8_t result = 0;
    SendRpcResponse(call_id, RpcStatus::ERROR, &result, sizeof(result));
    return;
  }
  uint8_t new_val = Value ? 1 : 0;
  palWriteLine(pin->line, new_val ? PAL_HIGH : PAL_LOW);
  if (pin->subscribed && new_val != pin->last_value) {
    EmitGpioEvent(pin->id, pin->last_value, new_val, 0);
  }
  pin->last_value = new_val;
  uint8_t result = 1;
  SendRpcResponse(call_id, RpcStatus::SUCCESS, &result, sizeof(result));
}

void RemoteGPIOService::RPCSubscribeGPIO(uint16_t call_id, uint8_t GPIOID, uint8_t Periodic) {
  auto* pin = FindGpio(GPIOID);
  if (!pin) {
    uint8_t result = 0;
    SendRpcResponse(call_id, RpcStatus::ERROR, &result, sizeof(result));
    return;
  }
  pin->subscribed = true;
  pin->periodic = Periodic != 0;
  uint8_t result = 1;
  SendRpcResponse(call_id, RpcStatus::SUCCESS, &result, sizeof(result));
}

void RemoteGPIOService::RPCUnsubscribeGPIO(uint16_t call_id, uint8_t GPIOID) {
  auto* pin = FindGpio(GPIOID);
  if (!pin) {
    uint8_t result = 0;
    SendRpcResponse(call_id, RpcStatus::ERROR, &result, sizeof(result));
    return;
  }
  pin->subscribed = false;
  pin->periodic = false;
  uint8_t result = 1;
  SendRpcResponse(call_id, RpcStatus::SUCCESS, &result, sizeof(result));
}

void RemoteGPIOService::RPCUnsubscribeAll(uint16_t call_id) {
  ClearSubscriptions();
  SendRpcResponse(call_id, RpcStatus::SUCCESS, nullptr, 0);
}

// ─── I2C RPCs ────────────────────────────────────────────────────────────────

static uint8_t MsgToI2cResult(msg_t msg) {
  if (msg == MSG_OK) return static_cast<uint8_t>(I2cResult::OK);
  if (msg == MSG_TIMEOUT) return static_cast<uint8_t>(I2cResult::ERR_TIMEOUT);
  return static_cast<uint8_t>(I2cResult::ERR_UNKNOWN);
}

// Response layout (written into framework-provided buffer):
//   [int32_t count_or_error][count bytes of data]
// count_or_error >= 0: bytes received; < 0: negated I2cResult error code.
static void FillReceiveResponse(msg_t msg, const uint8_t* rx_buf, uint8_t count, uint8_t* data,
                                uint16_t* response_length) {
  auto* hdr = reinterpret_cast<int32_t*>(data);
  if (msg == MSG_OK) {
    *hdr = static_cast<int32_t>(count);
    memcpy(data + sizeof(int32_t), rx_buf, count);
    *response_length = static_cast<uint16_t>(sizeof(int32_t) + count);
  } else {
    *hdr = -static_cast<int32_t>(MsgToI2cResult(msg));
    *response_length = sizeof(int32_t);
  }
}

void RemoteGPIOService::RPCI2cTransmit(uint16_t call_id, uint8_t BusID, uint8_t Address, const uint8_t* Data,
                                       uint32_t DataLen) {
  auto* bus = FindBus(BusID);
  if (!bus) {
    uint8_t result = static_cast<uint8_t>(I2cResult::ERR_BUS);
    SendRpcResponse(call_id, RpcStatus::ERROR, &result, sizeof(result));
    return;
  }
  i2cAcquireBus(bus->driver);
  msg_t msg = i2cMasterTransmit(bus->driver, Address, Data, DataLen, nullptr, 0);
  i2cReleaseBus(bus->driver);
  uint8_t result = MsgToI2cResult(msg);
  RpcStatus status = (msg == MSG_OK) ? RpcStatus::SUCCESS : RpcStatus::ERROR;
  SendRpcResponse(call_id, status, &result, sizeof(result));
}

void RemoteGPIOService::RPCI2cReceive(uint16_t call_id, uint8_t BusID, uint8_t Address, uint8_t Count, uint8_t* data,
                                      uint16_t* response_length) {
  (void)call_id;
  auto* bus = FindBus(BusID);
  if (!bus) {
    *reinterpret_cast<int32_t*>(data) = -static_cast<int32_t>(I2cResult::ERR_BUS);
    *response_length = sizeof(int32_t);
    return;
  }
  uint8_t rx_buf[kI2CReadBufferSize];
  uint8_t count = etl::min<uint8_t>(Count, kI2CReadBufferSize);
  i2cAcquireBus(bus->driver);
  msg_t msg = i2cMasterReceive(bus->driver, Address, rx_buf, count);
  i2cReleaseBus(bus->driver);
  FillReceiveResponse(msg, rx_buf, count, data, response_length);
}

void RemoteGPIOService::RPCI2cTransmitReceive(uint16_t call_id, uint8_t BusID, uint8_t Address, const uint8_t* TxData,
                                              uint32_t TxDataLen, uint8_t RxCount, uint8_t* data,
                                              uint16_t* response_length) {
  (void)call_id;
  auto* bus = FindBus(BusID);
  if (!bus) {
    *reinterpret_cast<int32_t*>(data) = -static_cast<int32_t>(I2cResult::ERR_BUS);
    *response_length = sizeof(int32_t);
    return;
  }
  uint8_t rx_buf[kI2CReadBufferSize];
  uint8_t rx_count = etl::min<uint8_t>(RxCount, kI2CReadBufferSize);
  i2cAcquireBus(bus->driver);
  msg_t msg = i2cMasterTransmit(bus->driver, Address, TxData, TxDataLen, rx_buf, rx_count);
  i2cReleaseBus(bus->driver);
  FillReceiveResponse(msg, rx_buf, rx_count, data, response_length);
}
