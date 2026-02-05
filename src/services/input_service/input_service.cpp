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
  num_active_lift_ = 0;

  // Add virtual inputs.
  lift_multiple_input_ = &all_inputs_.emplace_back();
  lift_multiple_input_->idx = Input::VIRTUAL;
  lift_multiple_input_->emergency_reason = EmergencyReason::LIFT_MULTIPLE | EmergencyReason::LATCH;
  lift_multiple_input_->emergency_delay_ms = LiftMultipleDelay.value;

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
          ULOG_ERROR("Too many inputs (max. %d)", all_inputs_.max_size() - NUM_VIRTUAL_INPUTS);
          return false;
        }
        data->current_input = &all_inputs_.emplace_back();
        data->current_input->idx = data->next_idx++;
        data->driver->AddInput(data->current_input);
      } else {
        // TODO: Give driver a chance to check completeness of the input?
        data->current_input = nullptr;
      }
      break;
    }

    // Value inside InputConfig (key in 4, value in 5)
    case 5: {
      const char* key = jsp->stack[4].meta.name;
      if (strcmp(key, "invert") == 0) {
        return JsonGetBool(type, data->current_input->invert);
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
            // A single sensor can only trigger a LIFT emergency.
            // It takes two or more "lift" sensors to cause a LIFT_MULTIPLE emergency.
            data->current_input->emergency_reason |= EmergencyReason::LIFT;
          } else if (strcmp(reason, "collision") == 0) {
            data->current_input->emergency_reason |= EmergencyReason::COLLISION;
          } else {
            ULOG_ERROR("Unknown emergency reason \"%s\"", reason);
            return false;
          }
          return true;
        } else if (strcmp(key, "delay") == 0) {
          return JsonGetNumber(jsp, type, data->current_input->emergency_delay_ms);
        } else if (strcmp(key, "latch") == 0) {
          if (type == LWJSON_STREAM_TYPE_FALSE) {
            data->current_input->emergency_reason &= ~EmergencyReason::LATCH;
            return true;
          } else if (type == LWJSON_STREAM_TYPE_TRUE) {
            return true;
          } else {
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

uint32_t InputService::OnLoop(uint32_t, uint32_t) {
  eventmask_t events = chEvtGetAndClearEvents(Events::ids_to_mask({Events::GPIO_TRIGGERED}));
  if (events & EVENT_MASK(Events::GPIO_TRIGGERED)) {
    auto* gpio_driver = static_cast<GpioInputDriver*>(drivers_.find("gpio")->second);
    gpio_driver->tick();
  }
  return UINT32_MAX;
}

void InputService::SendStatus() {
  uint64_t active_inputs_mask = 0;
  for (auto& input : all_inputs_) {
    if (input.IsActive() && input.idx != input.VIRTUAL) {
      active_inputs_mask |= 1 << input.idx;
    }
  }

  SendActiveInputs(active_inputs_mask);
}

bool InputService::SendInputEventHelper(Input& input, InputEventType type) {
  uint8_t payload[2] = {input.idx, static_cast<uint8_t>(type)};
  return SendInputEvent(payload, 2);
}

void InputService::OnInputChanged(Input& input, const bool active, const uint32_t duration) {
  if (input.idx == Input::VIRTUAL) return;

  if ((input.emergency_reason & EmergencyReason::LIFT) != 0) {
    uint8_t num_active_lift = active ? ++num_active_lift_ : --num_active_lift_;
    lift_multiple_input_->Update(num_active_lift >= 2);
  }

  // TODO: This will be called in the middle of the driver's update loop.
  //       We might want to queue the raw changes and send them at a safe time.
  StartTransaction();
  if (active) {
    SendInputEventHelper(input, InputEventType::ACTIVE);
  } else {
    SendInputEventHelper(input, InputEventType::INACTIVE);
    if (duration >= LongPressTime.value * 1000) {
      SendInputEventHelper(input, InputEventType::LONG);
    } else if (duration >= DebounceTime.value * 1000) {
      // Note that debouncing works only one-way here.
      // If the input becomes inactive for just a nanosecond, it will interrupt the long press.
      SendInputEventHelper(input, InputEventType::SHORT);
    }
  }
  CommitTransaction();
}

void InputService::OnSimulatedInputsChanged([[maybe_unused]] const uint64_t& new_value) {
#ifdef DEBUG_BUILD
  auto* simulated_driver = static_cast<SimulatedInputDriver*>(drivers_.find("simulated")->second);
  simulated_driver->SetActiveInputs(new_value);
#endif
}

etl::pair<uint16_t, uint32_t> InputService::GetEmergencyReasons(uint32_t now) {
  // Although the input states are atomic, the vector of inputs is not.
  Lock lk(&mutex_);
  uint16_t reasons = 0;
  uint32_t block_time = UINT32_MAX;
  for (auto& input : all_inputs_) {
    if (input.emergency_reason == 0) continue;
    if (input.GetAndClearPendingEmergency()) {
      reasons |= input.emergency_reason;
      block_time = 0;
    } else if (input.IsActive() &&
               TimeoutReached(input.ActiveDuration(now), input.emergency_delay_ms * 1'000, block_time)) {
      reasons |= input.emergency_reason;
    }
  }
  return {reasons, block_time};
}
