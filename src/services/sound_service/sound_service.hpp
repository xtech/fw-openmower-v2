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

  /* RPCs (services/sound_service.json): let a host audition sounds at runtime.
     Return 1 = accepted, 0 = rejected. */
  void RPCPlaySound(uint16_t call_id, SoundId Sound) override;
  void RPCPlayTone(uint16_t call_id, uint16_t Freq, uint16_t DurationMs, uint8_t Volume) override;
  void RPCPlaySequence(uint16_t call_id, const char* Sequence, uint32_t SequenceLen, Waveform Wave, uint8_t Volume,
                       uint8_t Unison, uint16_t DetuneHz) override;
  void RPCPlayMp3(uint16_t call_id, const char* Path, uint32_t PathLen) override;
  void RPCStop(uint16_t call_id) override;

  THD_WORKING_AREA(wa, 3072){};
};

#endif  // SOUND_SERVICE_HPP
