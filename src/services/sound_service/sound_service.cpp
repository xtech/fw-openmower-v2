#include "sound_service.hpp"

#include <drivers/gps/gps_driver.h>
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

void SoundService::OnVolumeChanged(const int8_t& new_value) {
  /* A negative value means "keep what the firmware has stored" (the high level sends -1
     by default), so only an explicit 0..100 touches the player. The value is clamped here,
     not only inside set_volume(), because the Volume output has to report what is actually
     played. */
  if (new_value < 0) {
    return;
  }
  const uint8_t volume = (new_value > 100) ? 100U : static_cast<uint8_t>(new_value);
  volume_ = volume;
  set_volume(volume);
  SendVolume(volume);
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

void SoundService::RPCPlaySequence(uint16_t call_id, const uint8_t* Sequence, uint32_t SequenceLen, Waveform Wave,
                                   uint8_t Volume, uint8_t Unison, uint16_t DetuneHz, uint8_t AttackMs, uint8_t DecayMs,
                                   uint8_t Preempt, uint16_t RepeatMs) {
  /* blob parameter: the text stays in the received packet, nothing is copied. */
  const char* text = reinterpret_cast<const char*>(Sequence);
  /* Note array stays on this thread's stack: kMaxNotes * 8 B = 64 B. */
  Note notes[kMaxNotes]{};
  const uint8_t count = parse_sequence(text, SequenceLen, notes, kMaxNotes);
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
          static_cast<int>(SequenceLen), text, count, Waveform_to_string(Wave), Volume, Unison, DetuneHz, AttackMs,
          DecayMs, RepeatMs);
    } else {
      ULOG_WARNING("Sound: RPC sequence rejected (%s)", play_result_text(res));
    }
  } else {
    /* %.*s takes an int width, hence the cast. */
    ULOG_WARNING("Sound: RPC sequence rejected (%.*s)", static_cast<int>(SequenceLen), text);
  }
  SendRpcResponse(call_id, xbot::datatypes::RpcStatus::SUCCESS, &result, sizeof(result));
}

void SoundService::RPCPlayMp3(uint16_t call_id, const char* Path, uint32_t PathLen, uint8_t Preempt) {
  uint8_t result = 0U;
  if (PathLen > 0U) {
    const PlayResult res = play_file(Path, Preempt != 0U);
    result = play_result_accepted(res) ? 1U : 0U;
    ULOG_INFO("Sound: RPC mp3 '%s' (%s)", Path, play_result_text(res));
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
struct ReasonKind {
  uint16_t reasons;  ///< Any of these bits selects the kind; the table order is the priority
  EmergencyKind kind;
};

/** @brief The audible reasons and the kind they announce. */
static constexpr ReasonKind kReasonKinds[] = {
    {EmergencyReason::STOP, EmergencyKind::STOP},
    {EmergencyReason::LIFT | EmergencyReason::LIFT_MULTIPLE, EmergencyKind::LIFT},
    {EmergencyReason::COLLISION | EmergencyReason::COLLISION_MULTIPLE, EmergencyKind::COLLISION},
    {EmergencyReason::MOWER_RPM_LIMIT, EmergencyKind::RPM_LIMIT},
    {EmergencyReason::MOWER_RPM_TIMEOUT, EmergencyKind::RPM_TIMEOUT},
};

/**
 * @brief Which kind an audible condition is (the sound side of the reason bits).
 *
 * The table order is the priority: STOP wins over LIFT wins over COLLISION wins over the mower
 * protections. "Audible" *is* the table, so a reason without an entry stays silent.
 */
static EmergencyKind KindOf(uint16_t reasons) {
  for (const auto& rule : kReasonKinds) {
    if ((reasons & rule.reasons) != 0U) {
      return rule.kind;
    }
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
static Signals HlStateOf(HighLevelStatus state_id, const char* name) {
  if (name != nullptr) {
    if (strcmp(name, "IDLE") == 0) return HL_MODE_IDLE;
    if (strcmp(name, "UNDOCKING") == 0) return HL_MODE_UNDOCKING;
    if (strcmp(name, "MOWING") == 0) return HL_MODE_MOWING;
    if (strcmp(name, "PAUSED") == 0) return HL_MODE_PAUSED;
    if (strcmp(name, "DOCKING") == 0) return HL_MODE_DOCKING;
    if (strcmp(name, "AREA_RECORDING") == 0) return HL_MODE_RECORDING;
  }
  // Fallback when the behaviour name is new
  switch (state_id) {
    case HighLevelStatus::IDLE: return HL_MODE_IDLE;
    case HighLevelStatus::AUTONOMOUS: return HL_MODE_AUTONOMOUS;
    case HighLevelStatus::RECORDING: return HL_MODE_RECORDING;
    default: return HL_MODE_UNKNOWN;
  }
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

Signals SoundService::ReadSignals() {
  Signals s = 0U;

  // Emergency
  const uint16_t reasons = emergency_service.GetEmergencyReasons();
  if ((reasons & EmergencyReason::TIMEOUT_HIGH_LEVEL) == 0U) s |= HL_ALIVE;
  s |= EmergencyKindBits(KindOf(reasons));
  if ((reasons & EmergencyReason::LATCH) != 0U) s |= EMERGENCY_LATCHED;

  // What the high level is doing: its behaviour name is what the announcements follow.
  const etl::string<100> name = high_level_service.GetStateName();
  s |= HlStateOf(high_level_service.GetStateId(), name.c_str());

  // Mowing
  if (robot != nullptr && robot->NeedsService(xbot::service_ids::MOWER) && mower_service.IsMowerRunning()) {
    s |= BLADE_RUNNING;
  }

  // Charging: the charger phase is two facts here (charging, done); UNKNOWN/FAULT/IDLE is silent.
  if (robot != nullptr && robot->NeedsService(xbot::service_ids::POWER)) {
    switch (ChargerStateOf(power_service.GetChargerStatus())) {
      case ChargerState::CHARGING: s |= CHARGING; break;
      case ChargerState::DONE: s |= CHARGER_DONE; break;
      default: break;
    }
  }

  // GPS RTK: the local receiver state is the source of the fix/lost announcements.
  if (robot != nullptr && robot->NeedsService(xbot::service_ids::GPS) &&
      gps_service.GetGpsRtkMode() == GpsRtkMode::RTK_FIX) {
    s |= RTK_FIX;
  }

  return s;
}

void SoundService::StopPlayback() {
  stop();
  ULOG_INFO("Sound: stop (alert=%s hl=%s charging=%u blade=%u)", alert_name(machine_.alert()),
            hl_name(machine_.hl_state()), machine_.charging() ? 1U : 0U, machine_.blade_running() ? 1U : 0U);
}

void SoundService::PlaySound(SoundId id) {
  const PlayResult res = play_sound_id(id);
  ULOG_INFO("Sound: play '%s' (alert=%s hl=%s charging=%u blade=%u, %s)", SoundId_to_string(id),
            alert_name(machine_.alert()), hl_name(machine_.hl_state()), machine_.charging() ? 1U : 0U,
            machine_.blade_running() ? 1U : 0U, play_result_text(res));
}

/**
 * @brief Log every change of a sound-relevant input, whether a sound follows or not.
 *
 * This is the debugging trace: a mower that stays silent is diagnosed from its state history
 */
void SoundService::LogSignalChanges(const Signals now) {
  const Signals was = last_signals_;
  const Signals changed = static_cast<Signals>(now ^ was);

  if (changed & HL_ALIVE) {
    ULOG_DEBUG("Sound: input link: %u -> %u", (was & HL_ALIVE) ? 1U : 0U, (now & HL_ALIVE) ? 1U : 0U);
  }
  if (HlMode(now) != HlMode(was)) {
    ULOG_DEBUG("Sound: input hl: %s -> %s", hl_name(was), hl_name(now));
  }
  if (changed & BLADE_RUNNING) {
    ULOG_DEBUG("Sound: input blade: %u -> %u", (was & BLADE_RUNNING) ? 1U : 0U, (now & BLADE_RUNNING) ? 1U : 0U);
  }
  if (changed & (CHARGING | CHARGER_DONE)) {
    ULOG_DEBUG("Sound: input charger: %u/%u -> %u/%u", (was & CHARGING) ? 1U : 0U, (was & CHARGER_DONE) ? 1U : 0U,
               (now & CHARGING) ? 1U : 0U, (now & CHARGER_DONE) ? 1U : 0U);
  }
  if (changed & RTK_FIX) {
    ULOG_DEBUG("Sound: input gps_rtk: fix=%u -> fix=%u", (was & RTK_FIX) ? 1U : 0U, (now & RTK_FIX) ? 1U : 0U);
  }
  if (EmergencyKindOf(now) != EmergencyKindOf(was)) {
    ULOG_DEBUG("Sound: input emergency: %s -> %s (latched=%u)", emergency_kind_name(EmergencyKindOf(was)),
               emergency_kind_name(EmergencyKindOf(now)), (now & EMERGENCY_LATCHED) ? 1U : 0U);
  }

  last_signals_ = now;
}

uint32_t SoundService::OnLoop(uint32_t now_micros, uint32_t /*last_tick_micros*/) {
  /* Wrapping microseconds in, monotonic milliseconds out (see now_ms_). Keep the
     unconverted sub-millisecond remainder in last_loop_micros_, so repeated 100 ms
     ticks cannot lose up to 999 us each time. */
  const uint32_t elapsed_us = now_micros - last_loop_micros_;
  const uint32_t elapsed_ms = elapsed_us / 1000U;
  now_ms_ += elapsed_ms;
  last_loop_micros_ = now_micros - (elapsed_us % 1000U);

  /* Nothing here needs the packet queue: the machine is the only periodic work, so the
     loop simply wakes up for it. Update() performs its stop/play calls through the
     output hooks; the player queue preserves their order. */
  const Signals now = ReadSignals();
  LogSignalChanges(now);
  machine_.Update(now, now_ms_);
  return kStateMachineTickUs;
}
