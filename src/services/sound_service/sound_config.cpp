/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "sound_config.hpp"

#include <etl/delegate.h>
#include <lwjson/lwjson.h>
#include <ulog.h>

#include <cstdlib>
#include <cstring>
#include <drivers/sound/sound_definition.hpp>
#include <drivers/sound/sound_storage.hpp>
#include <drivers/sound/sound_variants.hpp>
#include <json_stream.hpp>

namespace xbot::driver::sound {

namespace {

/** The parsed values carried between the streaming callbacks. */
struct ParseState : public json_data_t {
  SoundId current_id = SoundId::BOOT_PING;
  SoundDefinition current_def{};
  bool have_current = false;
  bool apply = true;            ///< Apply overrides and ambient policies (second pass)
  bool store_variants = false;  ///< Stage take lists while validating (first pass)
  uint8_t note_idx = 0;
  uint8_t num_sounds = 0;              ///< successfully parsed + applied definitions
  uint8_t num_ambient = 0;             ///< definitions that carry an every/chance
  uint32_t current_every_ms = 0;       ///< "every_ms" of the current sound
  uint8_t current_chance_pct = 0;      ///< "chance" (0..1 in the YAML) of the current sound
  char first_take[kVariantNameMax]{};  ///< take 1, used when the blob sends no "file"
};

/** every/chance per sound (shared by the parser and the state machine's policy hook). */
AmbientPolicy s_ambient[SoundId_count]{};

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

// --- Root / sounds object ---------------------------------------------------

/** @brief Validate the root object shape. */
bool OnRoot(ParseState& /*data*/, lwjson_stream_parser_t* /*jsp*/, lwjson_stream_type_t type) {
  JsonExpectTypeOrEnd(OBJECT);
  return true;
}

/** @brief Validate the "sounds" object shape. */
bool OnSoundsObject(ParseState& /*data*/, lwjson_stream_parser_t* /*jsp*/, lwjson_stream_type_t type) {
  JsonExpectTypeOrEnd(OBJECT);
  return true;
}

/** @brief Require the single "sounds" root key. */
bool OnRootKey(ParseState& /*data*/, lwjson_stream_parser_t* jsp, lwjson_stream_type_t type) {
  JsonExpectType(KEY);
  if (strcmp(jsp->data.str.buff, "sounds") != 0) {
    ULOG_ERROR("Sound config: expected \"sounds\" root key");
    return false;
  }
  return true;
}

// --- One sound --------------------------------------------------------------

/** @brief Start one sound definition and reset its per-sound parser state. */
bool OnSoundKey(ParseState& data, lwjson_stream_parser_t* jsp, lwjson_stream_type_t type) {
  JsonExpectType(KEY);
  SoundId id;
  if (!EnumFromString<SoundId, SoundId_count, SoundId_to_string>(jsp->data.str.buff, id)) {
    ULOG_ERROR("Sound config: unknown sound \"%s\"", jsp->data.str.buff);
    return false;
  }
  data.current_id = id;
  data.current_def = SoundDefinition{};
  data.first_take[0] = '\0';
  data.have_current = true;
  data.note_idx = 0;
  data.current_every_ms = 0U;
  data.current_chance_pct = 0U;
  return true;
}

/**
 * @brief Finish one sound object.
 *
 * The definition is validated here. The first (staging) pass only stages its takes and
 * leaves the runtime overrides untouched; the second pass applies the definition and its
 * ambient policy after the whole blob has passed validation.
 */
bool OnSoundDefinition(ParseState& data, lwjson_stream_parser_t* /*jsp*/, lwjson_stream_type_t type) {
  JsonExpectTypeOrEnd(OBJECT);
  if (type == LWJSON_STREAM_TYPE_OBJECT_END) {
    if (data.have_current && data.current_def.type == SoundType::SEQUENCE) {
      if (data.note_idx == 0U) {
        // A SEQUENCE without notes cannot produce a sound
        ULOG_ERROR("Sound config: SEQUENCE needs at least one note");
        return false;
      }
      data.current_def.sequence.count = data.note_idx;
    }
    /* A producer may send only the take list: the first take is the same MP3 a
       single-take definition would name, so it becomes the path. An explicit
       "file" always wins (it arrives before this point in the JSON). */
    if (data.current_def.type == SoundType::MP3 && data.current_def.path[0] == '\0' && data.first_take[0] != '\0' &&
        strlen(data.first_take) + sizeof("/sounds/") <= kMaxPath) {
      strcpy(data.current_def.path, "/sounds/");
      strcat(data.current_def.path, data.first_take);
    }
    if (data.apply) {
      set_sound_override(data.current_id, data.current_def);
      /* The blob carries milliseconds; the machine keeps seconds (see AmbientPolicy).
         Rounded half up, so `every: 60s` stays 60 — soundctl rejects < 1 s anyway. */
      const uint32_t every_s = (data.current_every_ms + 500U) / 1'000U;
      s_ambient[static_cast<uint8_t>(data.current_id)] =
          AmbientPolicy{static_cast<uint16_t>(every_s > 0xFFFFU ? 0xFFFFU : every_s), data.current_chance_pct};
      if (data.current_every_ms >= 500U && data.current_chance_pct != 0U) {
        data.num_ambient++;
      }
      data.num_sounds++;
    }
    data.have_current = false;
  }
  return true;
}

/** @brief Parse one field of the current sound definition. */
bool OnSoundField(ParseState& data, lwjson_stream_parser_t* jsp, lwjson_stream_type_t type) {
  if (!data.have_current) return true;
  const char* key = jsp->stack[5].meta.name;
  if (strcmp(key, "type") == 0) {
    JsonExpectType(STRING);
    SoundType t;
    if (!EnumFromString<SoundType, SoundType_count, SoundType_to_string>(jsp->data.str.buff, t)) {
      ULOG_ERROR("Sound config: unknown type \"%s\"", jsp->data.str.buff);
      return false;
    }
    data.current_def.type = t;
  } else if (strcmp(key, "volume") == 0) {
    uint16_t v = 0;
    if (!JsonGetNumber(jsp, type, v) || v > 100) {
      ULOG_ERROR("Sound config: invalid volume");
      return false;
    }
    data.current_def.volume = static_cast<uint8_t>(v);
  } else if (strcmp(key, "waveform") == 0) {
    JsonExpectType(STRING);
    Waveform w;
    if (!EnumFromString<Waveform, Waveform_count, Waveform_to_string>(jsp->data.str.buff, w)) {
      ULOG_ERROR("Sound config: unknown waveform \"%s\"", jsp->data.str.buff);
      return false;
    }
    data.current_def.waveform = w;
  } else if (strcmp(key, "unison") == 0) {
    uint16_t u = 0;
    if (!JsonGetNumber(jsp, type, u) || (u != 1 && u != 3 && u != 5 && u != 7)) {
      ULOG_ERROR("Sound config: invalid unison (expected 1/3/5/7)");
      return false;
    }
    data.current_def.unison = static_cast<uint8_t>(u);
  } else if (strcmp(key, "detune_hz") == 0) {
    uint16_t d = 0;
    if (!JsonGetNumber(jsp, type, d)) {
      ULOG_ERROR("Sound config: invalid detune_hz");
      return false;
    }
    data.current_def.detune_hz = d;
  } else if (strcmp(key, "attack_ms") == 0) {
    // Envelope: a fade-in at the start of every note (sequence sounds).
    uint16_t attack = 0;
    if (!JsonGetNumber(jsp, type, attack) || attack > 255U) {
      ULOG_ERROR("Sound config: invalid attack_ms (expected 0..255)");
      return false;
    }
    data.current_def.sequence.attack_ms = static_cast<uint8_t>(attack);
  } else if (strcmp(key, "decay_ms") == 0) {
    // Envelope: a note that fades away instead of being cut off (0 = hold).
    uint16_t decay = 0;
    if (!JsonGetNumber(jsp, type, decay) || decay > 255U) {
      ULOG_ERROR("Sound config: invalid decay_ms (expected 0..255)");
      return false;
    }
    data.current_def.sequence.decay_ms = static_cast<uint8_t>(decay);
  } else if (strcmp(key, "repeat_ms") == 0) {
    // Repeat the sound every n ms until another sound plays (0 = play once).
    uint16_t repeat = 0;
    if (!JsonGetNumber(jsp, type, repeat)) {
      ULOG_ERROR("Sound config: invalid repeat_ms");
      return false;
    }
    data.current_def.repeat_ms = repeat;
  } else if (strcmp(key, "pause_after_ms") == 0) {
    // Silence after this sound before the next one starts (0 = play through).
    uint16_t pause = 0;
    if (!JsonGetNumber(jsp, type, pause)) {
      ULOG_ERROR("Sound config: invalid pause_after_ms");
      return false;
    }
    data.current_def.pause_after_ms = pause;
  } else if (strcmp(key, "file") == 0) {
    JsonExpectType(STRING);
    const char* name = jsp->data.str.buff;
    // FW prefixes the hardcoded LL base path; the HL only sends the file name.
    if (strlen(name) + sizeof("/sounds/") > kMaxPath) {
      ULOG_ERROR("Sound config: file name too long \"%s\"", name);
      return false;
    }
    strcpy(data.current_def.path, "/sounds/");
    strcat(data.current_def.path, name);
  } else if (strcmp(key, "variants") == 0) {
    // Several takes: the names go straight into the staged store and are read
    // back when the sound plays (no RAM list, see sound_player.hpp).
    JsonExpectTypeOrEnd(ARRAY);
    if (data.store_variants) {
      if (type == LWJSON_STREAM_TYPE_ARRAY) {
        begin_sound_variant_set(data.current_id);
      } else {
        end_sound_variant_set();
      }
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
    if (data.store_variants) {
      set_sound_variant_pick(data.current_id, pick);
    }
  } else if (strcmp(key, "every_ms") == 0) {
    // Ambient sound: the low level rolls by itself whether to play at all. The rate
    // arrives in ms and is applied together with "chance" (see the object end above).
    uint32_t every = 0;
    if (!JsonGetNumber(jsp, type, every)) {
      ULOG_ERROR("Sound config: invalid every_ms");
      return false;
    }
    data.current_every_ms = every;
  } else if (strcmp(key, "chance") == 0) {
    JsonExpectType(NUMBER);
    const float chance = strtof(jsp->data.prim.buff, nullptr);
    if (chance < 0.0F || chance > 1.0F) {
      ULOG_ERROR("Sound config: invalid chance (expected 0..1)");
      return false;
    }
    data.current_chance_pct = static_cast<uint8_t>(chance * 100.0F + 0.5F);
  } else if (strcmp(key, "tone") == 0) {
    JsonExpectTypeOrEnd(OBJECT);
  } else if (strcmp(key, "sequence") == 0) {
    JsonExpectTypeOrEnd(ARRAY);
    if (type == LWJSON_STREAM_TYPE_ARRAY) {
      data.note_idx = 0;
    }
  } else {
    ULOG_ERROR("Sound config: unknown attribute \"%s\"", key);
    return false;
  }
  return true;
}

// --- Nested tone / sequence / take objects ----------------------------------

/** @brief Parse one tone/sequence item or a take-list object. */
bool OnToneOrItem(ParseState& data, lwjson_stream_parser_t* jsp, lwjson_stream_type_t type) {
  const char* parent = jsp->stack[5].meta.name;
  if (strcmp(parent, "tone") == 0) {
    JsonExpectType(KEY);  // tone field key; its value is handled in OnToneField.
  } else if (strcmp(parent, "sequence") == 0) {
    JsonExpectTypeOrEnd(OBJECT);
    if (type == LWJSON_STREAM_TYPE_OBJECT) {
      if (data.note_idx >= kMaxNotes) {
        ULOG_ERROR("Sound config: too many notes in sequence (max %hhu)", kMaxNotes);
        return false;
      }
      data.current_def.sequence.notes[data.note_idx] = Note{};
    } else {
      data.note_idx++;
    }
  } else if (strcmp(parent, "variants") == 0) {
    // One take object per array element; the name itself arrives in OnTakeOrNoteField.
    JsonExpectTypeOrEnd(OBJECT);
  }
  return true;
}

/** @brief Parse a field of a tone or a take object. */
bool OnToneField(ParseState& data, lwjson_stream_parser_t* jsp, lwjson_stream_type_t type) {
  if (strcmp(jsp->stack[5].meta.name, "tone") == 0) {
    const char* key = jsp->stack[7].meta.name;
    if (strcmp(key, "freq") == 0) {
      uint32_t f = 0;
      if (!JsonGetNumber(jsp, type, f) || f > 0xFFFFU) {
        ULOG_ERROR("Sound config: invalid tone freq (0..65535)");
        return false;
      }
      data.current_def.tone.freq = static_cast<uint16_t>(f);
    } else if (strcmp(key, "duration_ms") == 0) {
      uint32_t d = 0;
      if (!JsonGetNumber(jsp, type, d) || d > 0xFFFFU) {
        ULOG_ERROR("Sound config: invalid tone duration_ms (0..65535)");
        return false;
      }
      data.current_def.tone.duration_ms = static_cast<uint16_t>(d);
    }
    // Unknown tone sub-fields are ignored (forward compatibility).
  } else if (strcmp(jsp->stack[5].meta.name, "variants") == 0) {
    JsonExpectType(KEY);  // "file"; its value is handled in OnTakeOrNoteField.
  }
  // When parent is "sequence", stack_pos 8 is a note field key; its value
  // is handled in OnTakeOrNoteField.
  return true;
}

/** @brief Parse a take name or a sequence-note field. */
bool OnTakeOrNoteField(ParseState& data, lwjson_stream_parser_t* jsp, lwjson_stream_type_t type) {
  if (strcmp(jsp->stack[5].meta.name, "variants") == 0) {
    JsonExpectType(STRING);
    // The store keeps the bare name (the "/sounds/" prefix is the low level's
    // contract) and rejects a name or count that does not fit.
    const etl::string_view take(jsp->data.str.buff);
    if (data.store_variants && !add_sound_variant(take)) {
      return false;
    }
    if (data.first_take[0] == '\0') {
      for (size_t i = 0U; i < take.size() && i + 1U < kVariantNameMax; ++i) {
        data.first_take[i] = take[i];
      }
    }
    return true;
  }
  if (!data.have_current || data.note_idx >= kMaxNotes) return true;
  const char* key = jsp->stack[8].meta.name;
  Note& note = data.current_def.sequence.notes[data.note_idx];
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
  return true;
}

/**
 * @brief Dispatch one lwjson event to its named handler.
 *
 * The depths are the lwjson stream stack positions. Unused depths are ignored: the
 * parser is a fixed schema, so a new depth is an intentional code change here.
 */
bool OnEvent(lwjson_stream_parser_t* jsp, lwjson_stream_type_t type, void* data_voidptr) {
  auto* data = static_cast<ParseState*>(static_cast<json_data_t*>(data_voidptr));
  switch (jsp->stack_pos) {
    case 0: return OnRoot(*data, jsp, type);
    case 1: return OnRootKey(*data, jsp, type);
    case 2: return OnSoundsObject(*data, jsp, type);
    case 3: return OnSoundKey(*data, jsp, type);
    case 4: return OnSoundDefinition(*data, jsp, type);
    case 6: return OnSoundField(*data, jsp, type);
    case 7: return OnToneOrItem(*data, jsp, type);
    case 8: return OnToneField(*data, jsp, type);
    case 9: return OnTakeOrNoteField(*data, jsp, type);
    default: return true;
  }
}

}  // namespace

bool SoundConfig::Parse(const void* data, size_t length) {
  /* First pass: validate the complete schema and stage the take lists. Overrides and
     ambient policies are untouched here, so a rejected blob leaves the previous RAM
     configuration in place. */
  begin_sound_variants_write();

  ParseState state;
  state.apply = false;
  state.store_variants = true;
  state.callback = etl::delegate<bool(lwjson_stream_parser_t*, lwjson_stream_type_t, void*)>::create<&OnEvent>();
  xbot::service::HeatshrinkDataSource staging_source{static_cast<const uint8_t*>(data), length};
  if (!ProcessJson(staging_source, state)) {
    abort_sound_variants_write();
    ULOG_WARNING("Sound: definitions blob rejected (%zu byte blob)", length);
    return false;
  }

  /* The whole blob is valid and its takes are staged. Only now replace the previous
     RAM state; the second pass applies the definitions without touching the take store
     again. If that pass fails, it can only be an I/O error, not a schema error. */
  clear_sound_overrides();
  for (auto& policy : s_ambient) {
    policy = AmbientPolicy{};
  }

  state = ParseState{};
  state.callback = etl::delegate<bool(lwjson_stream_parser_t*, lwjson_stream_type_t, void*)>::create<&OnEvent>();
  xbot::service::HeatshrinkDataSource source{static_cast<const uint8_t*>(data), length};
  const bool accepted = ProcessJson(source, state);
  if (accepted) {
    // Persist the parsed overrides and takes to flash so they survive a reboot.
    commit_sound_variants_write();
    save_sound_overrides_to_storage();
    ULOG_INFO("Sound: %hhu definition(s) applied (%zu byte blob), %hhu with every/chance", state.num_sounds, length,
              state.num_ambient);
  } else {
    abort_sound_variants_write();
    ULOG_WARNING("Sound: definitions blob rejected (%zu byte blob)", length);
  }
  return accepted;
}

AmbientPolicy SoundConfig::PolicyFor(SoundId id) {
  const uint8_t idx = static_cast<uint8_t>(id);
  return idx < SoundId_count ? s_ambient[idx] : AmbientPolicy{};
}

}  // namespace xbot::driver::sound
