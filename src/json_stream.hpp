#include <etl/delegate.h>
#include <etl/to_arithmetic.h>
#include <lwjson/lwjson.h>
#include <ulog.h>

#include <cstddef>
#include <xbot-service/DataSource.hpp>

using xbot::service::DataSource;

struct json_data_t {
  etl::delegate<bool(lwjson_stream_parser_t*, lwjson_stream_type_t, void*)> callback;
  bool failed = false;
};

bool ProcessJson(DataSource& source, json_data_t& data);

#define JsonExpectType(expected)               \
  if (type != LWJSON_STREAM_TYPE_##expected) { \
    ULOG_ERROR("Expected type %s", #expected); \
    return false;                              \
  }

#define JsonExpectTypeOrEnd(expected)                                                         \
  if (type != LWJSON_STREAM_TYPE_##expected && type != LWJSON_STREAM_TYPE_##expected##_END) { \
    ULOG_ERROR("Expected type %s or %s_END", #expected, #expected);                           \
    return false;                                                                             \
  }

template <typename T>
bool JsonGetNumber(lwjson_stream_parser_t* jsp, lwjson_stream_type_t type, T& value) {
  JsonExpectType(NUMBER);
  etl::to_arithmetic_result result = etl::to_arithmetic<T>(jsp->data.prim.buff, strlen(jsp->data.prim.buff));
  if (result) {
    value = result.value();
    return true;
  } else {
    ULOG_ERROR("Failed to parse number from \"%s\"", jsp->data.prim.buff);
    return false;
  }
}
