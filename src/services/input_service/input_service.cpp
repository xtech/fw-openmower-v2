#include "input_service.hpp"

#include <ulog.h>

#include <drivers/input/gpio_input_driver.hpp>
#include <xbot-service/Lock.hpp>
#ifdef DEBUG_BUILD
#include <drivers/input/simulated_input_driver.hpp>
#endif
#include <globals.hpp>
#include <json_stream.hpp>

using xbot::service::Lock;

struct input_config_json_data_t : public json_data_t {
  InputDriver* driver = nullptr;
  Input* current_input = nullptr;
  uint8_t next_idx = 0;
};

bool InputService::OnRegisterInputConfigsChanged(const void* data, size_t length) {
  HeatshrinkDataSource source{static_cast<const uint8_t*>(data), length};
  Lock lk(&mutex_);

  all_inputs_.clear();
  for (auto& driver : drivers_) {
    driver.second->ClearInputs();
  }

  input_config_json_data_t json_data;
  json_data.callback = etl::make_delegate<InputService, &InputService::InputConfigsJsonCallback>(*this);
  return ProcessJson(source, json_data);
}

bool InputService::InputConfigsJsonCallback(lwjson_stream_parser_t* jsp, lwjson_stream_type_t type,
                                            void* data_voidptr) {
  auto* data = static_cast<input_config_json_data_t*>(data_voidptr);
  switch (jsp->stack_pos) {
    // Root
    case 0: JsonExpectTypeOrEnd(OBJECT); break;

    // Driver key
    case 1: {
      const char* driver = jsp->data.str.buff;
      if (strlen(driver) <= decltype(drivers_)::key_type::MAX_SIZE) {
        auto it = drivers_.find(driver);
        if (it == drivers_.end()) {
          ULOG_ERROR("Unknown input driver \"%s\"", driver);
          return false;
        }
        data->driver = it->second;
      } else {
        ULOG_ERROR("Unknown input driver \"%s\"", driver);
        return false;
      }
      break;
    }

    // List of inputs per driver
    case 2: JsonExpectTypeOrEnd(ARRAY); break;

    // Start/end of one InputConfig
    case 3: {
      JsonExpectTypeOrEnd(OBJECT);
      if (type == LWJSON_STREAM_TYPE_OBJECT) {
        if (all_inputs_.full()) {
          ULOG_ERROR("Too many inputs (max. %d)", all_inputs_.max_size());
          return false;
        }
        data->current_input = &data->driver->AddInput();
        data->current_input->idx = data->next_idx++;
      } else {
        // TODO: Give driver a chance to check completeness of the input?
        all_inputs_.push_back(data->current_input);
        data->current_input = nullptr;
      }
      break;
    }

    // Value inside InputConfig (key in 4, value in 5)
    case 5: {
      const char* key = jsp->stack[4].meta.name;
      if (strcmp(key, "name") == 0) {
        JsonExpectType(STRING);
        data->current_input->name = jsp->data.str.buff;
      } else if (strcmp(key, "emergency") == 0) {
        JsonExpectTypeOrEnd(OBJECT);
        if (type == LWJSON_STREAM_TYPE_OBJECT) {
          data->current_input->emergency_reason = EmergencyReason::LATCH;
        }
      } else if (!data->driver->OnInputConfigValue(jsp, key, type, *data->current_input)) {
        return false;
      }
      break;
    }

    // Value inside "emergency" (key in 6, value in 7)
    case 7: {
      const char* parent_key = jsp->stack[4].meta.name;
      const char* key = jsp->stack[6].meta.name;
      if (strcmp(parent_key, "emergency") == 0) {
        if (strcmp(key, "reason") == 0) {
          JsonExpectType(STRING);
          const char* reason = jsp->data.str.buff;
          if (strcmp(reason, "stop") == 0) {
            data->current_input->emergency_reason |= EmergencyReason::STOP;
          } else if (strcmp(reason, "lift") == 0) {
            data->current_input->emergency_reason |= EmergencyReason::LIFT;
          } else if (strcmp(reason, "collision") == 0) {
            data->current_input->emergency_reason |= EmergencyReason::COLLISION;
          } else {
            ULOG_ERROR("Unknown emergency reason \"%s\"", reason);
            return false;
          }
          return true;
        } else if (strcmp(key, "delay") == 0) {
          return JsonGetNumber(jsp, type, data->current_input->emergency_delay);
        } else if (strcmp(key, "latch") == 0) {
          if (type == LWJSON_STREAM_TYPE_FALSE) {
            data->current_input->emergency_reason &= ~EmergencyReason::LATCH;
            return true;
          } else if (type != LWJSON_STREAM_TYPE_TRUE) {
            ULOG_ERROR("Expected boolean");
            return false;
          }
        }
      }
      ULOG_ERROR("Unknown attribute \"%s.%s\"", parent_key, key);
      return false;
    }
  }
  return true;
}

bool InputService::OnStart() {
  // Start drivers.
  for (auto& driver : drivers_) {
    if (!driver.second->OnStart()) {
      ULOG_ERROR("Failed to start input driver \"%s\"", driver.first.c_str());
      return false;
    }
  }
  return true;
}

void InputService::OnStop() {
  // Stop drivers.
  for (auto& driver : drivers_) {
    driver.second->OnStop();
  }
}

void InputService::OnLoop(uint32_t, uint32_t) {
  eventmask_t events = chEvtGetAndClearEvents(Events::ids_to_mask({Events::GPIO_TRIGGERED}));
  if (events & EVENT_MASK(Events::GPIO_TRIGGERED)) {
    auto* gpio_driver = static_cast<GpioInputDriver*>(drivers_.find("gpio")->second);
    gpio_driver->tick();
  }
}

void InputService::SendStatus() {
  uint64_t active_inputs_mask = 0;
  for (auto& input : all_inputs_) {
    if (input->IsActive()) {
      active_inputs_mask |= 1 << input->idx;
    }
  }

  SendActiveInputs(active_inputs_mask);
}

bool InputService::SendInputEventHelper(Input& input, InputEventType type) {
  uint8_t payload[2] = {input.idx, static_cast<uint8_t>(type)};
  return SendInputEvent(payload, 2);
}

void InputService::OnInputChanged(Input& input) {
  // TODO: This will be called in the middle of the driver's update loop.
  //       We might want to queue the raw changes and send them at a safe time.
  StartTransaction();
  if (input.IsActive()) {
    SendInputEventHelper(input, InputEventType::ACTIVE);
  } else {
    SendInputEventHelper(input, InputEventType::INACTIVE);
    // TODO: This obviously needs debouncing, more variants and configuration.
    SendInputEventHelper(input, input.ActiveDuration() >= 500'000 ? InputEventType::LONG : InputEventType::SHORT);
  }
  CommitTransaction();
}

void InputService::OnSimulatedInputsChanged([[maybe_unused]] const uint64_t& new_value) {
#ifdef DEBUG_BUILD
  auto* simulated_driver = static_cast<SimulatedInputDriver*>(drivers_.find("simulated")->second);
  simulated_driver->SetActiveInputs(new_value);
#endif
}
