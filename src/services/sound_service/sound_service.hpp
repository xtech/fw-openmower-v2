#ifndef SOUND_SERVICE_HPP
#define SOUND_SERVICE_HPP

#include <etl/atomic.h>
#include <lwjson/lwjson.h>

#include <SoundServiceBase.hpp>
#include <drivers/sound/sound_state_machine.hpp>

using namespace xbot::service;
/** @brief The sound driver (StateMachine, StateInputs, …) — spelled `sound::` below. */
namespace sound = xbot::driver::sound;

struct sound_config_json_data_t;

class SoundService : public SoundServiceBase, public sound::SoundOutput {
 public:
  explicit SoundService(uint16_t service_id) : SoundServiceBase(service_id, wa, sizeof(wa)) {
  }

  bool IsHealthy() override {
    return IsRunning() && definitions_configured_;
  }

 private:
  etl::atomic<bool> definitions_configured_{false};

  /**
   * @brief Drives the announcements: a state change in, stop/play calls out.
   *
   * The input hooks are static functions of this class; the output is this service as
   * SoundOutput. The machine itself stays free of services and of the player (see
   * drivers/sound/sound_state_machine.hpp).
   */
  sound::StateMachine machine_{*this, {&SoundService::AmbientPolicyFor, &SoundService::Dice}};

  /**
   * @brief every/chance per sound, parsed from the definitions blob.
   *
   * RAM only (a 5-byte entry per id): the high level sends the blob on every connect, and
   * an ambience that plays before that simply has no rate yet.
   */
  static sound::AmbientPolicy s_ambient[SoundId_count];

  /** @brief every/chance of one sound as configured (the machine's policy hook). */
  static sound::AmbientPolicy AmbientPolicyFor(SoundId id);
  /** @brief true with the given probability (the machine's dice hook). */
  static bool Dice(uint8_t chance_pct);

  /** @brief Snapshot of the state the machine reacts to, read from the other services. */
  sound::StateInputs ReadInputs();
  /** @brief Log the sound-relevant inputs whenever they change (see the .cpp). */
  void LogInputChanges(const sound::StateInputs& now);

  /** @brief Output: stop the current playback and clear the player queue. */
  void StopPlayback() override;
  /** @brief Output: push one sound into the player queue (order is the play order). */
  void PlaySound(SoundId id) override;

  /**
   * @brief Monotonic millisecond clock for the machine, built from the loop's deltas.
   *
   * getTimeMicros() is a 32-bit microsecond counter (it wraps after ~71 minutes), and the
   * machine compares timestamps up to a minute apart — so the wrapping value is turned
   * into a monotonic one here.
   */
  /** @brief Previous snapshot, so the state trace only prints something that changed. */
  sound::StateInputs last_inputs_{};

  uint32_t last_loop_micros_ = 0;
  uint32_t now_ms_ = 0;

  uint32_t OnLoop(uint32_t now_micros, uint32_t last_tick_micros) override;

  bool OnRegisterSoundDefinitionsChanged(const void* data, size_t length) override;
  bool SoundDefinitionsJsonCallback(lwjson_stream_parser_t* jsp, lwjson_stream_type_t type, void* data);
  void OnVolumeChanged(const uint8_t& new_value) override;

  /* RPCs (services/sound_service.json): let a host audition sounds at runtime.
     Return 1 = accepted, 0 = rejected */
  void RPCPlaySound(uint16_t call_id, SoundId Sound) override;
  void RPCPlayTone(uint16_t call_id, uint16_t Freq, uint16_t DurationMs, uint8_t Volume, uint8_t Preempt) override;
  void RPCPlaySequence(uint16_t call_id, const char* Sequence, uint32_t SequenceLen, Waveform Wave, uint8_t Volume,
                       uint8_t Unison, uint16_t DetuneHz, uint8_t AttackMs, uint8_t DecayMs, uint8_t Preempt,
                       uint16_t RepeatMs) override;
  void RPCPlayMp3(uint16_t call_id, const char* Path, uint32_t PathLen, uint8_t Preempt) override;
  void RPCStop(uint16_t call_id) override;

  THD_WORKING_AREA(wa, 3072){};
};

#endif  // SOUND_SERVICE_HPP
