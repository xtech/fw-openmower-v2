#include <etl/delegate.h>
#include <lwjson/lwjson.h>

#include <cstddef>
#include <xbot-service/DataSource.hpp>

using xbot::service::DataSource;

struct json_data_t {
  etl::delegate<bool(lwjson_stream_parser_t*, lwjson_stream_type_t, void*)> callback;
  bool failed = false;
};

bool ProcessJson(DataSource& source, json_data_t& data);

#define JsonIsTypeOrEndType(type, expected) \
  (type == LWJSON_STREAM_TYPE_##expected || type == LWJSON_STREAM_TYPE_##expected##_END)

#define JsonExpectType(expected)               \
  if (type != LWJSON_STREAM_TYPE_##expected) { \
    ULOG_ERROR("Expected type %s", #expected); \
    return false;                              \
  }
