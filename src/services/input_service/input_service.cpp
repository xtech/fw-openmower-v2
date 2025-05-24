#include "input_service.hpp"

#include <ulog.h>

#include <xbot-service/Lock.hpp>

#include "../../globals.hpp"
#include "../../json_stream.hpp"

using xbot::service::Lock;

struct input_config_json_data_t : public json_data_t {
  InputDriver* driver = nullptr;
  Input* current_input = nullptr;
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
  if ((jsp->stack_pos == 0 && !JsonIsTypeOrEndType(type, OBJECT)) ||
      (jsp->stack_pos == 2 && !JsonIsTypeOrEndType(type, ARRAY)) ||
      (jsp->stack_pos == 3 && !JsonIsTypeOrEndType(type, OBJECT)) ||  //
      (jsp->stack_pos > 5)) {
    ULOG_ERROR("Invalid config structure");
    return false;
  }

  switch (jsp->stack_pos) {
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

    // Start/end of one InputConfig
    case 3: {
      if (type == LWJSON_STREAM_TYPE_OBJECT) {
        data->current_input = &data->driver->AddInput();
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
      } else if (strcmp(key, "emergency_type") == 0) {
        JsonExpectType(STRING);
        if (strcmp(jsp->data.str.buff, "emergency") == 0) {
          data->current_input->emergency_mode = Input::EmergencyMode::EMERGENCY;
        } else if (strcmp(jsp->data.str.buff, "trapped") == 0) {
          data->current_input->emergency_mode = Input::EmergencyMode::TRAPPED;
        } else if (strcmp(jsp->data.str.buff, "none") != 0) {
          ULOG_ERROR("Invalid emergency type \"%s\"", jsp->data.str.buff);
          return false;
        }
      } else if (!data->driver->OnInputConfigValue(jsp, key, type, *data->current_input)) {
        return false;
      }
      break;
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
    gpio_driver_.tick();
  }
}
