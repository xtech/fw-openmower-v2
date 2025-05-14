#include "json_stream.hpp"

#include <ulog.h>

static bool IsOnlyWhitespace(const char* json, size_t length, size_t start_pos) {
  for (size_t i = start_pos; i < length; i++) {
    if (strchr(" \t\r\n", json[i]) == nullptr) {
      return false;
    }
  }
  return true;
}

static bool IsError(lwjsonr_t res) {
  switch (res) {
    case lwjsonERR:
    case lwjsonERRJSON:
    case lwjsonERRMEM:
    case lwjsonERRPAR: return true;
    default: return false;
  }
}

static void LogErrorPosition(const char* json, size_t length, size_t error_pos) {
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

static void Callback(lwjson_stream_parser_t* jsp, lwjson_stream_type_t type) {
  auto* data = static_cast<json_data_t*>(lwjson_stream_get_user_data(jsp));
  if (!data->callback(jsp, type, data)) {
    data->failed = true;
  }
}

bool ProcessJson(const char* json, size_t length, json_data_t& data) {
  lwjson_stream_parser_t stream_parser;
  lwjson_stream_init(&stream_parser, Callback);
  lwjson_stream_set_user_data(&stream_parser, &data);

  for (size_t i = 0; i < length; i++) {
    lwjsonr_t res = lwjson_stream_parse(&stream_parser, json[i]);
    if (res == lwjsonSTREAMDONE) {
      if (IsOnlyWhitespace(json, length, i + 1)) {
        return true;
      } else {
        ULOG_ERROR("Input config JSON parsing failed: extra characters after end");
        LogErrorPosition(json, length, i + 1);
        return false;
      }
    } else if (data.failed) {
      LogErrorPosition(json, length, i + 1);
      return false;
    } else if (IsError(res)) {
      ULOG_ERROR("Input config JSON parsing failed");
      LogErrorPosition(json, length, i + 1);
      return false;
    }
  }
  ULOG_ERROR("Input config JSON parsing failed: end not found");
  LogErrorPosition(json, length, length);
  return true;
}
