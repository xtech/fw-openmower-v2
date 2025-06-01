#include "json_stream.hpp"

static bool OnlyWhitespaceRemaining(DataSource& json) {
  while (json.HasNext()) {
    if (strchr(" \t\r\n", json.Next()) == nullptr) {
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

static void LogErrorPosition(DataSource& json) {
  const size_t error_pos = json.Position();
  json.Rewind();
  // TODO: Can we directly allocate a packet here, so we don't need another temporary buffer?
  char line_buf[128];
  size_t line_pos = 0;
  while (json.HasNext()) {
    char c = json.Next();
    if (c != '\n') {
      line_buf[line_pos++] = c;
    }
    if (json.Position() == error_pos) {
      line_buf[line_pos++] = '<';
      line_buf[line_pos++] = 'E';
      line_buf[line_pos++] = 'R';
      line_buf[line_pos++] = 'R';
      line_buf[line_pos++] = 'O';
      line_buf[line_pos++] = 'R';
      line_buf[line_pos++] = '>';
    }
    bool split_line = line_pos >= sizeof(line_buf) - 7;
    if (split_line) {
      line_buf[line_pos++] = '<';
      line_buf[line_pos++] = 'S';
      line_buf[line_pos++] = 'P';
      line_buf[line_pos++] = 'L';
      line_buf[line_pos++] = 'I';
      line_buf[line_pos++] = 'T';
      line_buf[line_pos++] = '>';
    }
    if (c == '\n' || !json.HasNext() || split_line) {
      ULOG_ERROR("%.*s", line_pos, line_buf);
      line_pos = 0;
    }
  }
}

static void Callback(lwjson_stream_parser_t* jsp, lwjson_stream_type_t type) {
  auto* data = static_cast<json_data_t*>(lwjson_stream_get_user_data(jsp));
  if (!data->callback(jsp, type, data)) {
    data->failed = true;
  }
}

bool ProcessJson(DataSource& json, json_data_t& data) {
  lwjson_stream_parser_t stream_parser;
  lwjson_stream_init(&stream_parser, Callback);
  lwjson_stream_set_user_data(&stream_parser, &data);

  json.Rewind();
  while (json.HasNext()) {
    lwjsonr_t res = lwjson_stream_parse(&stream_parser, json.Next());
    if (res == lwjsonSTREAMDONE) {
      if (OnlyWhitespaceRemaining(json)) {
        return true;
      } else {
        ULOG_ERROR("Input config JSON parsing failed: extra characters after end");
        LogErrorPosition(json);
        return false;
      }
    } else if (data.failed) {
      LogErrorPosition(json);
      return false;
    } else if (IsError(res)) {
      ULOG_ERROR("Input config JSON parsing failed");
      LogErrorPosition(json);
      return false;
    }
  }
  ULOG_ERROR("Input config JSON parsing failed: end not found");
  LogErrorPosition(json);
  return true;
}

bool JsonGetBool(lwjson_stream_type_t type, bool& value) {
  if (type == LWJSON_STREAM_TYPE_TRUE) {
    value = true;
    return true;
  } else if (type == LWJSON_STREAM_TYPE_FALSE) {
    value = false;
    return true;
  } else {
    ULOG_ERROR("Expected boolean");
    return false;
  }
}
