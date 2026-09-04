#ifndef SOUND_SERVICE_HPP
#define SOUND_SERVICE_HPP

#include <etl/atomic.h>
#include <lwjson/lwjson.h>

#include <SoundServiceBase.hpp>

using namespace xbot::service;

struct sound_config_json_data_t;

class SoundService : public SoundServiceBase {
 public:
  explicit SoundService(uint16_t service_id) : SoundServiceBase(service_id, wa, sizeof(wa)) {
  }

  bool IsHealthy() override {
    return IsRunning() && definitions_configured_;
  }

 private:
  etl::atomic<bool> definitions_configured_{false};

  bool OnRegisterSoundDefinitionsChanged(const void* data, size_t length) override;
  bool SoundDefinitionsJsonCallback(lwjson_stream_parser_t* jsp, lwjson_stream_type_t type, void* data);
  void OnVolumeChanged(const uint8_t& new_value) override;

  THD_WORKING_AREA(wa, 3072){};
};

#endif  // SOUND_SERVICE_HPP
