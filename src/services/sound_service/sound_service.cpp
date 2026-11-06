#include "sound_service.hpp"

#include <service_ids.h>
#include <ulog.h>

#include <cstring>
#include <drivers/sound/sound_definition.hpp>
#include <drivers/sound/sound_player.hpp>
#include <drivers/sound/sound_sequence.hpp>
#include <json_stream.hpp>

#include "globals.hpp"
#include "services.hpp"

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

sound::AmbientPolicy SoundService::s_ambient[SoundId_count]{};

struct sound_config_json_data_t : public json_data_t {
  SoundId current_id = SoundId::BOOT_PING;
  SoundDefinition current_def{};
  bool have_current = false;
  uint8_t note_idx = 0;
  uint8_t num_sounds = 0;              ///< successfully parsed + applied definitions
  uint8_t num_ambient = 0;             ///< definitions that carry an every/chance
  uint32_t current_every_ms = 0;       ///< "every_ms" of the current sound
  uint8_t current_chance_pct = 0;      ///< "chance" (0..1 in the YAML) of the current sound
  char first_take[kVariantNameMax]{};  ///< take 1, used when the blob sends no "file"
};

namespace {

/**
 * @brief Parse an enum value from its codegen-generated string form.
 */
template <typename EnumT, size_t Count, const char* (*ToString)(EnumT)>
bool EnumFromString(const char* s, EnumT& out) {
  for (size_t i = 0U; i < Count; ++i) {
    const EnumT v = static_cast<EnumT>(i);
    if (strcmp(s, ToString(v)) == 0) {
      out = v;
      return true;
    }
  }
  return false;
}

}  // namespace

bool SoundService::OnRegisterSoundDefinitionsChanged(const void* data, size_t length) {
  HeatshrinkDataSource source{static_cast<const uint8_t*>(data), length};

  // A fresh blob fully replaces the previous configuration: clear any sound
  // overrides first so missing entries fall back to the ROM defaults.
  clear_sound_overrides();
  for (auto& policy : s_ambient) {
    policy = AmbientPolicy{};
  }
  // The take lists are staged: a rejected blob leaves the previous ones in place.
  begin_sound_variants_write();

  sound_config_json_data_t json_data;
  json_data.callback = etl::make_delegate<SoundService, &SoundService::SoundDefinitionsJsonCallback>(*this);
  definitions_configured_ = ProcessJson(source, json_data);
  if (definitions_configured_) {
    // Persist the parsed overrides and takes to flash so they survive a reboot.
    commit_sound_variants_write();
    save_sound_overrides_to_storage();
    ULOG_INFO("Sound: %hhu definition(s) applied (%zu byte blob), %hhu with every/chance", json_data.num_sounds, length,
              json_data.num_ambient);
    /* A new blob may carry a different `every`/`chance`: restart the ambience interval so
       it takes effect without waiting for the next state change. The clock is the same
       monotonic one the loop uses (this callback runs on the service thread). */
    machine_.AmbientPolicyChanged(now_ms_);
  } else {
    abort_sound_variants_write();
    ULOG_WARNING("Sound: definitions blob rejected (%zu byte blob)", length);
  }
  return definitions_configured_;
}

bool SoundService::SoundDefinitionsJsonCallback(lwjson_stream_parser_t* jsp, lwjson_stream_type_t type,
                                                void* data_voidptr) {
  auto* data = static_cast<sound_config_json_data_t*>(data_voidptr);
  switch (jsp->stack_pos) {
    // Root object.
    case 0: JsonExpectTypeOrEnd(OBJECT); break;

    // Key "sounds".
    case 1: {
      JsonExpectType(KEY);
      if (strcmp(jsp->data.str.buff, "sounds") != 0) {
        ULOG_ERROR("Sound config: expected \"sounds\" root key");
        return false;
      }
      break;
    }

    // "sounds" object.
    case 2: JsonExpectTypeOrEnd(OBJECT); break;

    // Sound name key.
    case 3: {
      JsonExpectType(KEY);
      SoundId id;
      if (!EnumFromString<SoundId, SoundId_count, SoundId_to_string>(jsp->data.str.buff, id)) {
        ULOG_ERROR("Sound config: unknown sound \"%s\"", jsp->data.str.buff);
        return false;
      }
      data->current_id = id;
      data->current_def = SoundDefinition{};
      data->first_take[0] = '\0';
      data->have_current = true;
      data->note_idx = 0;
      data->current_every_ms = 0U;
      data->current_chance_pct = 0U;
      break;
    }

    // Sound definition object.
    case 4: {
      JsonExpectTypeOrEnd(OBJECT);
      if (type == LWJSON_STREAM_TYPE_OBJECT_END) {
        if (data->have_current && data->current_def.type == SoundType::SEQUENCE) {
          if (data->note_idx == 0U) {
            // A SEQUENCE without notes cannot produce a sound
            ULOG_ERROR("Sound config: SEQUENCE needs at least one note");
            return false;
          }
          data->current_def.sequence.count = data->note_idx;
        }
        /* A producer may send only the take list: the first take is the same MP3 a
           single-take definition would name, so it becomes the path. An explicit
           "file" always wins (it arrives before this point in the JSON). */
        if (data->current_def.type == SoundType::MP3 && data->current_def.path[0] == '\0' &&
            data->first_take[0] != '\0' && strlen(data->first_take) + sizeof("/sounds/") <= kMaxPath) {
          strcpy(data->current_def.path, "/sounds/");
          strcat(data->current_def.path, data->first_take);
        }
        set_sound_override(data->current_id, data->current_def);
        /* The blob carries milliseconds; the machine keeps seconds (see AmbientPolicy).
           Rounded half up, so `every: 60s` stays 60 — soundctl rejects < 1 s anyway. */
        const uint32_t every_s = (data->current_every_ms + 500U) / 1'000U;
        s_ambient[static_cast<uint8_t>(data->current_id)] =
            AmbientPolicy{static_cast<uint16_t>(every_s > 0xFFFFU ? 0xFFFFU : every_s), data->current_chance_pct};
        if (data->current_every_ms >= 500U && data->current_chance_pct != 0U) {
          data->num_ambient++;
        }
        data->num_sounds++;
        data->have_current = false;
      }
      break;
    }

    // Field value inside a sound definition (key at stack[5]).
    case 6: {
      if (!data->have_current) break;
      const char* key = jsp->stack[5].meta.name;
      if (strcmp(key, "type") == 0) {
        JsonExpectType(STRING);
        SoundType t;
        if (!EnumFromString<SoundType, SoundType_count, SoundType_to_string>(jsp->data.str.buff, t)) {
          ULOG_ERROR("Sound config: unknown type \"%s\"", jsp->data.str.buff);
          return false;
        }
        data->current_def.type = t;
      } else if (strcmp(key, "volume") == 0) {
        uint16_t v = 0;
        if (!JsonGetNumber(jsp, type, v) || v > 100) {
          ULOG_ERROR("Sound config: invalid volume");
          return false;
        }
        data->current_def.volume = static_cast<uint8_t>(v);
      } else if (strcmp(key, "waveform") == 0) {
        JsonExpectType(STRING);
        Waveform w;
        if (!EnumFromString<Waveform, Waveform_count, Waveform_to_string>(jsp->data.str.buff, w)) {
          ULOG_ERROR("Sound config: unknown waveform \"%s\"", jsp->data.str.buff);
          return false;
        }
        data->current_def.waveform = w;
      } else if (strcmp(key, "unison") == 0) {
        uint16_t u = 0;
        if (!JsonGetNumber(jsp, type, u) || (u != 1 && u != 3 && u != 5 && u != 7)) {
          ULOG_ERROR("Sound config: invalid unison (expected 1/3/5/7)");
          return false;
        }
        data->current_def.unison = static_cast<uint8_t>(u);
      } else if (strcmp(key, "detune_hz") == 0) {
        uint16_t d = 0;
        if (!JsonGetNumber(jsp, type, d)) {
          ULOG_ERROR("Sound config: invalid detune_hz");
          return false;
        }
        data->current_def.detune_hz = d;
      } else if (strcmp(key, "attack_ms") == 0) {
        // Envelope: a fade-in at the start of every note (sequence sounds).
        uint16_t attack = 0;
        if (!JsonGetNumber(jsp, type, attack) || attack > 255U) {
          ULOG_ERROR("Sound config: invalid attack_ms (expected 0..255)");
          return false;
        }
        data->current_def.sequence.attack_ms = static_cast<uint8_t>(attack);
      } else if (strcmp(key, "decay_ms") == 0) {
        // Envelope: a note that fades away instead of being cut off (0 = hold).
        uint16_t decay = 0;
        if (!JsonGetNumber(jsp, type, decay) || decay > 255U) {
          ULOG_ERROR("Sound config: invalid decay_ms (expected 0..255)");
          return false;
        }
        data->current_def.sequence.decay_ms = static_cast<uint8_t>(decay);
      } else if (strcmp(key, "repeat_ms") == 0) {
        // Repeat the sound every n ms until another sound plays (0 = play once).
        uint16_t repeat = 0;
        if (!JsonGetNumber(jsp, type, repeat)) {
          ULOG_ERROR("Sound config: invalid repeat_ms");
          return false;
        }
        data->current_def.repeat_ms = repeat;
      } else if (strcmp(key, "pause_after_ms") == 0) {
        // Silence after this sound before the next one starts (0 = play through).
        uint16_t pause = 0;
        if (!JsonGetNumber(jsp, type, pause)) {
          ULOG_ERROR("Sound config: invalid pause_after_ms");
          return false;
        }
        data->current_def.pause_after_ms = pause;
      } else if (strcmp(key, "file") == 0) {
        JsonExpectType(STRING);
        const char* name = jsp->data.str.buff;
        // FW prefixes the hardcoded LL base path; the HL only sends the file name.
        if (strlen(name) + sizeof("/sounds/") > kMaxPath) {
          ULOG_ERROR("Sound config: file name too long \"%s\"", name);
          return false;
        }
        strcpy(data->current_def.path, "/sounds/");
        strcat(data->current_def.path, name);
      } else if (strcmp(key, "variants") == 0) {
        // Several takes: the names go straight into the staged store and are read
        // back when the sound plays (no RAM list, see sound_player.hpp).
        JsonExpectTypeOrEnd(ARRAY);
        if (type == LWJSON_STREAM_TYPE_ARRAY) {
          begin_sound_variant_set(data->current_id);
        } else {
          end_sound_variant_set();
        }
      } else if (strcmp(key, "pick") == 0) {
        JsonExpectType(STRING);
        const char* mode = jsp->data.str.buff;
        PickMode pick;
        if (strcmp(mode, "first") == 0) {
          pick = PickMode::FIRST;
        } else if (strcmp(mode, "random") == 0) {
          pick = PickMode::RANDOM;
        } else if (strcmp(mode, "rotate") == 0) {
          pick = PickMode::ROTATE;
        } else {
          ULOG_ERROR("Sound config: unknown pick \"%s\"", mode);
          return false;
        }
        set_sound_variant_pick(data->current_id, pick);
      } else if (strcmp(key, "every_ms") == 0) {
        // Ambient sound: the low level rolls by itself whether to play at all. The rate
        // arrives in ms and is applied together with "chance" (see the object end above).
        uint32_t every = 0;
        if (!JsonGetNumber(jsp, type, every)) {
          ULOG_ERROR("Sound config: invalid every_ms");
          return false;
        }
        data->current_every_ms = every;
      } else if (strcmp(key, "chance") == 0) {
        JsonExpectType(NUMBER);
        const float chance = strtof(jsp->data.prim.buff, nullptr);
        if (chance < 0.0F || chance > 1.0F) {
          ULOG_ERROR("Sound config: invalid chance (expected 0..1)");
          return false;
        }
        data->current_chance_pct = static_cast<uint8_t>(chance * 100.0F + 0.5F);
      } else if (strcmp(key, "tone") == 0) {
        JsonExpectTypeOrEnd(OBJECT);
      } else if (strcmp(key, "sequence") == 0) {
        JsonExpectTypeOrEnd(ARRAY);
        if (type == LWJSON_STREAM_TYPE_ARRAY) {
          data->note_idx = 0;
        }
      } else {
        ULOG_ERROR("Sound config: unknown attribute \"%s\"", key);
        return false;
      }
      break;
    }

    // tone field key, or a sequence note object.
    case 7: {
      const char* parent = jsp->stack[5].meta.name;
      if (strcmp(parent, "tone") == 0) {
        JsonExpectType(KEY);  // tone field key; value is handled in case 8.
      } else if (strcmp(parent, "sequence") == 0) {
        JsonExpectTypeOrEnd(OBJECT);
        if (type == LWJSON_STREAM_TYPE_OBJECT) {
          if (data->note_idx >= kMaxNotes) {
            ULOG_ERROR("Sound config: too many notes in sequence (max %hhu)", kMaxNotes);
            return false;
          }
          data->current_def.sequence.notes[data->note_idx] = Note{};
        } else {
          data->note_idx++;
        }
      } else if (strcmp(parent, "variants") == 0) {
        // One take object per array element; the name itself arrives in case 9.
        JsonExpectTypeOrEnd(OBJECT);
      }
      break;
    }

    // tone field value (parent "tone" at stack[5], key at stack[7]).
    case 8: {
      if (strcmp(jsp->stack[5].meta.name, "tone") == 0) {
        const char* key = jsp->stack[7].meta.name;
        if (strcmp(key, "freq") == 0) {
          uint32_t f = 0;
          if (!JsonGetNumber(jsp, type, f) || f > 0xFFFFU) {
            ULOG_ERROR("Sound config: invalid tone freq (0..65535)");
            return false;
          }
          data->current_def.tone.freq = static_cast<uint16_t>(f);
        } else if (strcmp(key, "duration_ms") == 0) {
          uint32_t d = 0;
          if (!JsonGetNumber(jsp, type, d) || d > 0xFFFFU) {
            ULOG_ERROR("Sound config: invalid tone duration_ms (0..65535)");
            return false;
          }
          data->current_def.tone.duration_ms = static_cast<uint16_t>(d);
        }
        // Unknown tone sub-fields are ignored (forward compatibility).
      } else if (strcmp(jsp->stack[5].meta.name, "variants") == 0) {
        JsonExpectType(KEY);  // "file"; the value is handled in case 9.
      }
      // When parent is "sequence", stack_pos 8 is a note field key; its value
      // is handled in case 9.
      break;
    }

    // Take file value ("variants": [{"file": "..."}]), or a sequence note field value.
    case 9: {
      if (strcmp(jsp->stack[5].meta.name, "variants") == 0) {
        JsonExpectType(STRING);
        // The store keeps the bare name (the "/sounds/" prefix is the low level's
        // contract) and rejects a name or count that does not fit.
        const etl::string_view take(jsp->data.str.buff);
        if (!add_sound_variant(take)) {
          return false;
        }
        if (data->first_take[0] == '\0') {
          for (size_t i = 0U; i < take.size() && i + 1U < kVariantNameMax; ++i) {
            data->first_take[i] = take[i];
          }
        }
        break;
      }
      if (!data->have_current || data->note_idx >= kMaxNotes) break;
      const char* key = jsp->stack[8].meta.name;
      Note& note = data->current_def.sequence.notes[data->note_idx];
      if (strcmp(key, "freq") == 0) {
        uint16_t f = 0;
        if (!JsonGetNumber(jsp, type, f)) {
          ULOG_ERROR("Sound config: invalid note freq");
          return false;
        }
        note.freq = f;
      } else if (strcmp(key, "duration_ms") == 0) {
        uint16_t d = 0;
        if (!JsonGetNumber(jsp, type, d)) {
          ULOG_ERROR("Sound config: invalid note duration_ms");
          return false;
        }
        note.duration_ms = d;
      } else if (strcmp(key, "lfo_hz_x10") == 0) {
        uint16_t v = 0;
        if (!JsonGetNumber(jsp, type, v)) {
          ULOG_ERROR("Sound config: invalid note lfo_hz_x10");
          return false;
        }
        note.lfo_hz_x10 = v;
      } else if (strcmp(key, "lfo_depth") == 0) {
        uint16_t v = 0;
        if (!JsonGetNumber(jsp, type, v)) {
          ULOG_ERROR("Sound config: invalid note lfo_depth");
          return false;
        }
        note.lfo_depth = v;
      }
      break;
    }
  }
  return true;
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
  const uint8_t result = (res == PlayResult::QUEUED) ? 1U : 0U;
  SendRpcResponse(call_id, xbot::datatypes::RpcStatus::SUCCESS, &result, sizeof(result));
}

void SoundService::RPCPlayTone(uint16_t call_id, uint16_t Freq, uint16_t DurationMs, uint8_t Volume, uint8_t Preempt) {
  const PlayResult res = play_tone(Freq, DurationMs, Volume, Preempt != 0U);
  ULOG_INFO("Sound: RPC tone %hu Hz %hu ms vol %hhu preempt=%hhu (%s, playing=%u)", Freq, DurationMs, Volume, Preempt,
            play_result_text(res), is_playing() ? 1U : 0U);
  const uint8_t result = (res == PlayResult::QUEUED) ? 1U : 0U;
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
    result = (res == PlayResult::QUEUED) ? 1U : 0U;
    if (res == PlayResult::QUEUED) {
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
    result = (res == PlayResult::QUEUED) ? 1U : 0U;
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
  const uint8_t idx = static_cast<uint8_t>(id);
  return idx < SoundId_count ? s_ambient[idx] : AmbientPolicy{};
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
 * stay UNKNOWN, so a new behaviour in the high level never silences the existing ones - the
 * machine then falls back to the blade (see StateMachine::DrivingNoticeStep).
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

  /* Charging: any active phase counts, DONE is what CHARGING_DONE announces. */
  if (robot != nullptr && robot->NeedsService(xbot::service_ids::POWER)) {
    const CHARGER_STATUS charger = power_service.GetChargerStatus();
    in.charging = (charger == CHARGER_STATUS::TRICKLE) || (charger == CHARGER_STATUS::PRE_CHARGE) ||
                  (charger == CHARGER_STATUS::CC) || (charger == CHARGER_STATUS::CV) ||
                  (charger == CHARGER_STATUS::TOP_OFF);
    in.charge_done = (charger == CHARGER_STATUS::DONE);
    const float percent = power_service.GetBatteryPercent();
    if (percent >= 0.0F && percent <= 100.0F) {
      in.battery_percent = static_cast<uint8_t>(percent + 0.5F);
    }
  }

  return in;
}

void SoundService::PlayAction(const StateAction& action) {
  if (action.stop_first) stop();
  if (action.silent()) {
    /* Nothing to announce, only to stop (the warning device ended): stop() above did it. */
    ULOG_INFO("Sound: silenced (alert=%s hl=%s charging=%u blade=%u)", alert_name(machine_.alert()),
              hl_state_name(machine_.hl_state()), machine_.charging() ? 1U : 0U, machine_.blade_running() ? 1U : 0U);
    return;
  }

  const PlayResult res = play_sound_id(action.sound);
  ULOG_INFO("Sound: announced '%s' (alert=%s hl=%s charging=%u blade=%u, %s)", SoundId_to_string(action.sound),
            alert_name(machine_.alert()), hl_state_name(machine_.hl_state()), machine_.charging() ? 1U : 0U,
            machine_.blade_running() ? 1U : 0U, play_result_text(res));

  if (action.follow_up != kNoSound) {
    /* The announcement's `pause_after_ms` (YAML) spaces the two. */
    const PlayResult then_res = play_sound_id(action.follow_up);
    ULOG_INFO("Sound: queued '%s' after it (%s)", SoundId_to_string(action.follow_up), play_result_text(then_res));
  }
}

/**
 * @brief Log every change of a sound-relevant input, whether a sound follows or not.
 *
 * This is the debugging trace: a mower that stays silent is diagnosed from its state
 * history, not from its sounds. One line per changed input keeps it greppable
 * (`Sound: input …`) and costs nothing while everything is steady - which is most of the
 * time on a robot that stands in the garden.
 *
 * The machine additionally *debounces* the blade and the charger (kInputDebounceMs); the
 * raw changes are logged here because a bouncing signal is exactly what one wants to see
 * when a sound is late.
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
  if (now.charging != was.charging) {
    ULOG_INFO("Sound: input charging: %u -> %u", was.charging ? 1U : 0U, now.charging ? 1U : 0U);
  }
  if (now.charge_done != was.charge_done) {
    ULOG_INFO("Sound: input charge_done: %u -> %u", was.charge_done ? 1U : 0U, now.charge_done ? 1U : 0U);
  }
  if (now.emergency != was.emergency) {
    ULOG_INFO("Sound: input emergency: %s -> %s", emergency_kind_name(was.emergency),
              emergency_kind_name(now.emergency));
  }
  if (now.emergency_latched != was.emergency_latched) {
    ULOG_INFO("Sound: input emergency_latched: %u -> %u", was.emergency_latched ? 1U : 0U,
              now.emergency_latched ? 1U : 0U);
  }
  if (now.battery_percent != was.battery_percent) {
    ULOG_INFO("Sound: input battery: %u -> %u %%", was.battery_percent, now.battery_percent);
  }

  last_inputs_ = now;
}

uint32_t SoundService::OnLoop(uint32_t now_micros, uint32_t /*last_tick_micros*/) {
  /* Wrapping microseconds in, monotonic milliseconds out (see now_ms_). */
  now_ms_ += (now_micros - last_loop_micros_) / 1000U;
  last_loop_micros_ = now_micros;

  /* Nothing here needs the packet queue: the machine is the only periodic work, so the
     loop simply wakes up for it. */
  const StateInputs in = ReadInputs();
  LogInputChanges(in);
  const StateAction action = machine_.Update(in, now_ms_);
  if (!action.silent() || action.stop_first) {
    PlayAction(action);
  }
  return kStateMachineTickUs;
}
