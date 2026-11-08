#include "sound_service.hpp"

#include <service_ids.h>
#include <ulog.h>

#include <cstring>
#include <drivers/sound/sound_definition.hpp>
#include <drivers/sound/sound_player.hpp>
#include <drivers/sound/sound_sequence.hpp>

#include "globals.hpp"
#include "services.hpp"
#include "sound_config.hpp"

using namespace xbot::driver::sound;

/**
 * @brief How often the state machine looks at the world.
 *
 * The *loop* wants microseconds (Service::OnLoop returns its block time in µs, and the
 * framework clamps it to 1 s), while everything the machine itself handles is
 * milliseconds. Keeping the period in ms and converting here makes that obvious.
 */
static constexpr uint32_t kStateMachineTickMs = 100U;
static constexpr uint32_t kStateMachineTickUs = kStateMachineTickMs * 1'000U;

/**
 * @brief Which emergency reasons are announced with a sound.
 *
 * The real dangers: the physical conditions from the input service (their configured
 * `emergency_reason` masks carry EmergencyReason::LATCH together with the condition bit,
 * and only once the input's delay elapsed — see InputService::GetEmergencyReasons) and
 * the mower RPM cutoffs (MowerService / MowingBehavior).
 *
 * Infrastructure/state reasons (TIMEOUT_*, SERVICE_NOT_READY, HIGH_LEVEL, ...) stay
 * silent: the link state has its own ROS_CONNECTED/ROS_DISCONNECTED sounds, and the high
 * level's initial latched state at connect is normal operation, not an alert.
 */
static constexpr uint16_t kAudibleReasons =
    EmergencyReason::STOP | EmergencyReason::LIFT | EmergencyReason::LIFT_MULTIPLE | EmergencyReason::COLLISION |
    EmergencyReason::COLLISION_MULTIPLE | EmergencyReason::MOWER_RPM_LIMIT | EmergencyReason::MOWER_RPM_TIMEOUT;

bool SoundService::OnRegisterSoundDefinitionsChanged(const void* data, size_t length) {
  const bool accepted = SoundConfig::Parse(data, length);
  definitions_configured_ = accepted;
  if (accepted) {
    /* A new blob may carry a different `every`/`chance`: restart the ambience interval so
       it takes effect without waiting for the next state change. The clock is the same
       monotonic one the loop uses (this callback runs on the service thread). */
    machine_.AmbientPolicyChanged(now_ms_);
  }
  return accepted;
}

void SoundService::OnVolumeChanged(const uint8_t& new_value) {
  set_volume(new_value);
}

/*---------------------------------------------------------------------------
 * RPCs (services/sound_service.json) — runtime auditioning for hosts.
 * Each one logs what it did: that is the only feedback a host gets, because the
 * response goes to the service owner (the high-level system), not to the caller.
 *---------------------------------------------------------------------------*/

void SoundService::RPCPlaySound(uint16_t call_id, SoundId Sound) {
  const PlayResult res = play_sound_id(Sound);
  ULOG_INFO("Sound: RPC sound '%s' (%s, playing=%u)", SoundId_to_string(Sound), play_result_text(res),
            is_playing() ? 1U : 0U);
  const uint8_t result = play_result_accepted(res) ? 1U : 0U;
  SendRpcResponse(call_id, xbot::datatypes::RpcStatus::SUCCESS, &result, sizeof(result));
}

void SoundService::RPCPlayTone(uint16_t call_id, uint16_t Freq, uint16_t DurationMs, uint8_t Volume, uint8_t Preempt) {
  const PlayResult res = play_tone(Freq, DurationMs, Volume, Preempt != 0U);
  ULOG_INFO("Sound: RPC tone %hu Hz %hu ms vol %hhu preempt=%hhu (%s, playing=%u)", Freq, DurationMs, Volume, Preempt,
            play_result_text(res), is_playing() ? 1U : 0U);
  const uint8_t result = play_result_accepted(res) ? 1U : 0U;
  SendRpcResponse(call_id, xbot::datatypes::RpcStatus::SUCCESS, &result, sizeof(result));
}

void SoundService::RPCPlaySequence(uint16_t call_id, const char* Sequence, uint32_t SequenceLen, Waveform Wave,
                                   uint8_t Volume, uint8_t Unison, uint16_t DetuneHz, uint8_t AttackMs, uint8_t DecayMs,
                                   uint8_t Preempt, uint16_t RepeatMs) {
  /* Note array stays on this thread's stack: kMaxNotes * 8 B = 64 B. */
  Note notes[kMaxNotes]{};
  const uint8_t count = parse_sequence(Sequence, SequenceLen, notes, kMaxNotes);
  uint8_t result = 0U;
  if (count > 0U) {
    const PlayResult res =
        play_sequence(notes, count, Wave, Volume, Unison, DetuneHz, AttackMs, DecayMs, RepeatMs, Preempt != 0U);
    result = play_result_accepted(res) ? 1U : 0U;
    if (play_result_accepted(res)) {
      // The only feedback a host gets (the response goes to the service owner), so log everything
      ULOG_INFO(
          "Sound: RPC sequence '%.*s' (%hhu notes, %s, vol %hhu, unison %hhu, detune %hu, attack %hhu ms, "
          "decay %hhu ms, repeat %hu ms)",
          static_cast<int>(SequenceLen), Sequence, count, Waveform_to_string(Wave), Volume, Unison, DetuneHz, AttackMs,
          DecayMs, RepeatMs);
    } else {
      ULOG_WARNING("Sound: RPC sequence rejected (%s)", play_result_text(res));
    }
  } else {
    /* %.*s takes an int width, hence the cast. */
    ULOG_WARNING("Sound: RPC sequence rejected (%.*s)", static_cast<int>(SequenceLen), Sequence);
  }
  SendRpcResponse(call_id, xbot::datatypes::RpcStatus::SUCCESS, &result, sizeof(result));
}

void SoundService::RPCPlayMp3(uint16_t call_id, const char* Path, uint32_t PathLen, uint8_t Preempt) {
  char path[kMaxPath];
  size_t n = (PathLen < (kMaxPath - 1U)) ? PathLen : (kMaxPath - 1U);
  memcpy(path, Path, n);
  path[n] = '\0';

  uint8_t result = 0U;
  if (n > 0U) {
    const PlayResult res = play_file(path, Preempt != 0U);
    result = play_result_accepted(res) ? 1U : 0U;
    ULOG_INFO("Sound: RPC mp3 '%s' (%s)", path, play_result_text(res));
  } else {
    ULOG_WARNING("Sound: RPC mp3 rejected (empty path)");
  }
  SendRpcResponse(call_id, xbot::datatypes::RpcStatus::SUCCESS, &result, sizeof(result));
}

void SoundService::RPCStop(uint16_t call_id) {
  stop();
  ULOG_INFO("Sound: RPC stop (queues flushed)");
  const uint8_t result = 1U;
  SendRpcResponse(call_id, xbot::datatypes::RpcStatus::SUCCESS, &result, sizeof(result));
}

/*---------------------------------------------------------------------------*\
 * Sound state machine
 *
 * The services own the state and the player owns the audio; the machine
 * (drivers/sound/sound_state_machine.*) maps a state transition to a sound.
 * The plumbing below is all this file has to do for it: fill the inputs from
 * the services, apply the decision, log the announcement.
\*---------------------------------------------------------------------------*/

AmbientPolicy SoundService::AmbientPolicyFor(SoundId id) {
  return SoundConfig::PolicyFor(id);
}

bool SoundService::Dice(uint8_t chance_pct) {
  return random_chance(chance_pct);
}

/**
 * @brief Which emergency condition the reasons describe.
 *
 * Only one condition names the episode (the machine announces it once).
 */
static EmergencyKind KindOf(uint16_t reasons) {
  if ((reasons & EmergencyReason::STOP) != 0U) {
    return EmergencyKind::STOP;
  }
  if ((reasons & (EmergencyReason::LIFT | EmergencyReason::LIFT_MULTIPLE)) != 0U) {
    return EmergencyKind::LIFT;
  }
  if ((reasons & (EmergencyReason::COLLISION | EmergencyReason::COLLISION_MULTIPLE)) != 0U) {
    return EmergencyKind::COLLISION;
  }
  if ((reasons & EmergencyReason::MOWER_RPM_LIMIT) != 0U) {
    return EmergencyKind::RPM_LIMIT;
  }
  if ((reasons & EmergencyReason::MOWER_RPM_TIMEOUT) != 0U) {
    return EmergencyKind::RPM_TIMEOUT;
  }
  if ((reasons & kAudibleReasons) != 0U) {
    return EmergencyKind::OTHER;
  }
  return EmergencyKind::NONE;
}

/**
 * @brief Which of the high level's behaviours the announced state belongs to.
 *
 * The high level sends its behaviour name, which is what the announcements follow: the
 * mode alone (IDLE/AUTONOMOUS/RECORDING) cannot tell undocking from docking. Unknown names
 * stay UNKNOWN, so a new behaviour in the high level never silences the existing ones; the
 * machine then simply has no announcement for it.
 */
static HlState HlStateOf(HighLevelStatus state_id, const char* name) {
  if (name != nullptr) {
    if (strcmp(name, "IDLE") == 0) return HlState::IDLE;
    if (strcmp(name, "UNDOCKING") == 0) return HlState::UNDOCKING;
    if (strcmp(name, "MOWING") == 0) return HlState::MOWING;
    if (strcmp(name, "PAUSED") == 0) return HlState::PAUSED;
    if (strcmp(name, "DOCKING") == 0) return HlState::DOCKING;
    if (strcmp(name, "AREA_RECORDING") == 0) return HlState::RECORDING;
  }
  if (state_id == HighLevelStatus::RECORDING) return HlState::RECORDING;
  return HlState::UNKNOWN;
}

/**
 * @brief Map the charger driver status to the one value the sound logic needs.
 *
 * Only an active phase is "charging", DONE is the end of a charge; a fault or a comms
 * error is UNKNOWN (no sound, the power service reports the fault itself).
 */
static ChargerState ChargerStateOf(CHARGER_STATUS status) {
  switch (status) {
    case CHARGER_STATUS::TRICKLE:
    case CHARGER_STATUS::PRE_CHARGE:
    case CHARGER_STATUS::CC:
    case CHARGER_STATUS::CV:
    case CHARGER_STATUS::TOP_OFF: return ChargerState::CHARGING;
    case CHARGER_STATUS::DONE: return ChargerState::DONE;
    case CHARGER_STATUS::NOT_CHARGING: return ChargerState::IDLE;
    case CHARGER_STATUS::FAULT:
    case CHARGER_STATUS::COMMS_ERROR:
    case CHARGER_STATUS::UNKNOWN: break;
  }
  return ChargerState::UNKNOWN;
}

StateInputs SoundService::ReadInputs() {
  StateInputs in{};

  /* Emergency: the high level timeout is the link, the audible bits are the danger. Every
     audible condition arrives together with EmergencyReason::LATCH once it lasted its
     configured delay, so a flickering sensor never reaches the machine. */
  /* What the high level is doing: its behaviour name is what the announcements follow. */
  const etl::string<100> hl_name = high_level_service.GetStateName();
  in.hl_state = HlStateOf(high_level_service.GetStateId(), hl_name.c_str());

  const uint16_t reasons = emergency_service.GetEmergencyReasons();
  in.high_level_alive = (reasons & EmergencyReason::TIMEOUT_HIGH_LEVEL) == 0U;
  in.emergency = KindOf(reasons);
  in.emergency_latched = (reasons & EmergencyReason::LATCH) != 0U;

  /* Mowing: the blade turns. MowerService owns that definition (it is the same value it
     reports as `MowerRunning`), so the sound logic needs no RPM threshold of its own. */
  if (robot != nullptr && robot->NeedsService(xbot::service_ids::MOWER)) {
    in.blade_running = mower_service.IsMowerRunning();
  }

  /* Charging: one enum instead of two booleans (the states are mutually exclusive). */
  if (robot != nullptr && robot->NeedsService(xbot::service_ids::POWER)) {
    in.charger = ChargerStateOf(power_service.GetChargerStatus());
  }

  return in;
}

void SoundService::StopPlayback() {
  stop();
  ULOG_INFO("Sound: stop (alert=%s hl=%s charging=%u blade=%u)", alert_name(machine_.alert()),
            hl_state_name(machine_.hl_state()), machine_.charging() ? 1U : 0U, machine_.blade_running() ? 1U : 0U);
}

void SoundService::PlaySound(SoundId id) {
  const PlayResult res = play_sound_id(id);
  ULOG_INFO("Sound: play '%s' (alert=%s hl=%s charging=%u blade=%u, %s)", SoundId_to_string(id),
            alert_name(machine_.alert()), hl_state_name(machine_.hl_state()), machine_.charging() ? 1U : 0U,
            machine_.blade_running() ? 1U : 0U, play_result_text(res));
}

/**
 * @brief Log every change of a sound-relevant input, whether a sound follows or not.
 *
 * This is the debugging trace: a mower that stays silent is diagnosed from its state
 * history, not from its sounds. One line per changed input keeps it greppable
 * (`Sound: input …`) and costs nothing while everything is steady - which is most of the
 * time on a robot that stands in the garden.
 *
 * The machine follows the inputs as the services report them; filtering belongs to the
 * owning service (the charger is polled once per second, the mower state already comes
 * from a fresh ESC sample). The raw changes are logged here because a bouncing signal is
 * exactly what one wants to see when a sound is late.
 */
void SoundService::LogInputChanges(const StateInputs& now) {
  const StateInputs& was = last_inputs_;

  if (now.high_level_alive != was.high_level_alive) {
    ULOG_INFO("Sound: input link: %u -> %u", was.high_level_alive ? 1U : 0U, now.high_level_alive ? 1U : 0U);
  }
  if (now.hl_state != was.hl_state) {
    ULOG_INFO("Sound: input hl: %s -> %s", hl_state_name(was.hl_state), hl_state_name(now.hl_state));
  }
  if (now.blade_running != was.blade_running) {
    ULOG_INFO("Sound: input blade: %u -> %u", was.blade_running ? 1U : 0U, now.blade_running ? 1U : 0U);
  }
  if (now.charger != was.charger) {
    ULOG_INFO("Sound: input charger: %s -> %s", charger_state_name(was.charger), charger_state_name(now.charger));
  }
  if (now.emergency != was.emergency) {
    ULOG_INFO("Sound: input emergency: %s -> %s", emergency_kind_name(was.emergency),
              emergency_kind_name(now.emergency));
  }
  if (now.emergency_latched != was.emergency_latched) {
    ULOG_INFO("Sound: input emergency_latched: %u -> %u", was.emergency_latched ? 1U : 0U,
              now.emergency_latched ? 1U : 0U);
  }

  last_inputs_ = now;
}

uint32_t SoundService::OnLoop(uint32_t now_micros, uint32_t /*last_tick_micros*/) {
  /* Wrapping microseconds in, monotonic milliseconds out (see now_ms_). */
  now_ms_ += (now_micros - last_loop_micros_) / 1000U;
  last_loop_micros_ = now_micros;

  /* Nothing here needs the packet queue: the machine is the only periodic work, so the
     loop simply wakes up for it. Update() performs its stop/play calls through the
     output hooks; the player queue preserves their order. */
  const StateInputs in = ReadInputs();
  LogInputChanges(in);
  machine_.Update(in, now_ms_);
  return kStateMachineTickUs;
}
