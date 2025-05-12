#include "input_service.hpp"

#include <ulog.h>

#include <xbot-service/Lock.hpp>

#include "../../globals.hpp"

using xbot::service::Lock;

const char* JSON_WHITESPACE_CHARS = " \t\r\n";

struct input_config_json_data_t {
  InputService* obj = nullptr;
  InputDriver* driver = nullptr;
  Input* current_input = nullptr;
  bool failed = false;
};

static bool lwjson_is_error(lwjsonr_t res) {
  switch (res) {
    case lwjsonERR:
    case lwjsonERRJSON:
    case lwjsonERRMEM:
    case lwjsonERRPAR: return true;
    default: return false;
  }
}

static void LogJsonErrorPosition(const char* json, size_t length, size_t error_pos) {
  size_t line_start = 0;
  for (size_t i = 0; i < length; i++) {
    char c = json[i];
    if (c == '\n' || i == length - 1) {
      if (line_start <= error_pos && error_pos <= i) {
        ULOG_ERROR("%.*s<ERROR>%.*s", error_pos - line_start, &json[line_start], i - error_pos, &json[error_pos]);
      } else {
        ULOG_ERROR("%.*s", i - line_start, &json[line_start]);
      }
      line_start = i + 1;
    }
  }
}

static bool IsOnlyJsonWhitespace(const char* json, size_t length, size_t start_pos) {
  for (size_t i = start_pos; i < length; i++) {
    if (strchr(JSON_WHITESPACE_CHARS, json[i]) == nullptr) {
      return false;
    }
  }
  return true;
}

bool InputService::OnRegisterInputConfigsChanged(const void* data, size_t length) {
  Lock lk(&mutex_);

  all_inputs_.clear();
  for (auto& driver : drivers_) {
    driver.second->ClearInputs();
  }

  static lwjson_stream_parser_t stream_parser;
  input_config_json_data_t json_data{.obj = this};
  lwjson_stream_init(&stream_parser, InputService::InputConfigsJsonCallbackHelper);
  lwjson_stream_set_user_data(&stream_parser, &json_data);

  const char* json = static_cast<const char*>(data);
  for (size_t i = 0; i < length; i++) {
    lwjsonr_t res = lwjson_stream_parse(&stream_parser, json[i]);
    if (res == lwjsonSTREAMDONE) {
      if (IsOnlyJsonWhitespace(json, length, i + 1)) {
        return true;
      } else {
        ULOG_ERROR("Input config JSON parsing failed: extra characters after end");
        LogJsonErrorPosition(json, length, i + 1);
        return false;
      }
    } else if (json_data.failed) {
      LogJsonErrorPosition(json, length, i + 1);
      return false;
    } else if (lwjson_is_error(res)) {
      ULOG_ERROR("Input config JSON parsing failed");
      LogJsonErrorPosition(json, length, i + 1);
      return false;
    }
  }
  ULOG_ERROR("Input config JSON parsing failed: end not found");
  LogJsonErrorPosition(json, length, length);
  return true;
}

void InputService::InputConfigsJsonCallbackHelper(lwjson_stream_parser_t* jsp, lwjson_stream_type_t type) {
  input_config_json_data_t* data = static_cast<input_config_json_data_t*>(lwjson_stream_get_user_data(jsp));
  data->obj->InputConfigsJsonCallback(jsp, type, data);
}

#define lwjson_is_type_or_end(type, expected) \
  (type == LWJSON_STREAM_TYPE_##expected || type == LWJSON_STREAM_TYPE_##expected##_END)

#define lwjson_expect_type(expected)           \
  if (type != LWJSON_STREAM_TYPE_##expected) { \
    data->failed = true;                       \
    return;                                    \
  }

#define JSON_ERROR(...)    \
  ULOG_ERROR(__VA_ARGS__); \
  data->failed = true;     \
  return;

void InputService::InputConfigsJsonCallback(lwjson_stream_parser_t* jsp, lwjson_stream_type_t type,
                                            input_config_json_data_t* data) {
  if ((jsp->stack_pos == 0 && !lwjson_is_type_or_end(type, OBJECT)) ||
      (jsp->stack_pos == 2 && !lwjson_is_type_or_end(type, ARRAY)) ||
      (jsp->stack_pos == 3 && !lwjson_is_type_or_end(type, OBJECT)) ||  //
      (jsp->stack_pos > 5)) {
    JSON_ERROR("Invalid config structure");
  }

  switch (jsp->stack_pos) {
    // Driver key
    case 1: {
      const char* driver = jsp->data.str.buff;
      if (strlen(driver) <= decltype(drivers_)::key_type::MAX_SIZE) {
        auto it = drivers_.find(driver);
        if (it == drivers_.end()) {
          JSON_ERROR("Unknown input driver \"%s\"", driver);
        }
        data->driver = it->second;
      } else {
        JSON_ERROR("Unknown input driver \"%s\"", driver);
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
        lwjson_expect_type(STRING);
        data->current_input->name = jsp->data.str.buff;
      } else if (strcmp(key, "emergency_type") == 0) {
        lwjson_expect_type(STRING);
        if (strcmp(jsp->data.str.buff, "emergency") == 0) {
          data->current_input->emergency_mode = Input::EmergencyMode::EMERGENCY;
        } else if (strcmp(jsp->data.str.buff, "trapped") == 0) {
          data->current_input->emergency_mode = Input::EmergencyMode::TRAPPED;
        } else if (strcmp(jsp->data.str.buff, "none") != 0) {
          JSON_ERROR("Invalid emergency type \"%s\"", jsp->data.str.buff);
        }
      } else if (!data->driver->OnInputConfigValue(jsp, key, type, *data->current_input)) {
        data->failed = true;
        return;
      }
      break;
    }
  }
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
