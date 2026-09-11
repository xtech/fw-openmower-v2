#include "sound_service.hpp"

#include <ulog.h>

#include <cstring>
#include <drivers/sound/sound_definition.hpp>
#include <drivers/sound/sound_player.hpp>
#include <json_stream.hpp>

using namespace xbot::driver::sound;

struct sound_config_json_data_t : public json_data_t {
  SoundId current_id = SoundId::BOOT_PING;
  SoundDefinition current_def{};
  bool have_current = false;
  uint8_t note_idx = 0;
  uint8_t num_sounds = 0;  ///< successfully parsed + applied definitions
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

  sound_config_json_data_t json_data;
  json_data.callback = etl::make_delegate<SoundService, &SoundService::SoundDefinitionsJsonCallback>(*this);
  definitions_configured_ = ProcessJson(source, json_data);
  if (definitions_configured_) {
    // Persist the parsed overrides to flash so they survive a reboot.
    save_sound_overrides_to_storage();
    ULOG_INFO("Sound: %u definition(s) applied (%u byte blob)", static_cast<unsigned>(json_data.num_sounds),
              static_cast<unsigned>(length));
  } else {
    ULOG_WARNING("Sound: definitions blob rejected (%u byte blob)", static_cast<unsigned>(length));
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
      data->have_current = true;
      data->note_idx = 0;
      break;
    }

    // Sound definition object.
    case 4: {
      JsonExpectTypeOrEnd(OBJECT);
      if (type == LWJSON_STREAM_TYPE_OBJECT_END) {
        if (data->have_current && data->current_def.type == SoundType::SEQUENCE) {
          data->current_def.sequence.count = data->note_idx;
        }
        set_sound_override(data->current_id, data->current_def);
        data->num_sounds++;
        // INFO (not DEBUG): the remote log is filtered at ULOG_INFO_LEVEL, and this
        // line is the per-definition "did the parse really work?" evidence.
        if (data->current_def.type == SoundType::MP3) {
          ULOG_INFO("Sound: applied %s type=MP3 volume=%u file='%s'", SoundId_to_string(data->current_id),
                    static_cast<unsigned>(data->current_def.volume), data->current_def.path);
        } else if (data->current_def.type == SoundType::SEQUENCE) {
          ULOG_INFO("Sound: applied %s type=SEQUENCE volume=%u notes=%u", SoundId_to_string(data->current_id),
                    static_cast<unsigned>(data->current_def.volume),
                    static_cast<unsigned>(data->current_def.sequence.count));
        } else {
          ULOG_INFO("Sound: applied %s type=TONE volume=%u tone=%u Hz/%u ms", SoundId_to_string(data->current_id),
                    static_cast<unsigned>(data->current_def.volume), static_cast<unsigned>(data->current_def.tone.freq),
                    static_cast<unsigned>(data->current_def.tone.duration_ms));
        }
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
            ULOG_ERROR("Sound config: too many notes in sequence (max %u)", kMaxNotes);
            return false;
          }
          data->current_def.sequence.notes[data->note_idx] = Note{};
        } else {
          data->note_idx++;
        }
      }
      break;
    }

    // tone field value (parent "tone" at stack[5], key at stack[7]).
    case 8: {
      if (strcmp(jsp->stack[5].meta.name, "tone") == 0) {
        const char* key = jsp->stack[7].meta.name;
        if (strcmp(key, "freq") == 0) {
          uint32_t f = 0;
          if (!JsonGetNumber(jsp, type, f)) {
            ULOG_ERROR("Sound config: invalid tone freq");
            return false;
          }
          data->current_def.tone.freq = f;
        } else if (strcmp(key, "duration_ms") == 0) {
          uint32_t d = 0;
          if (!JsonGetNumber(jsp, type, d)) {
            ULOG_ERROR("Sound config: invalid tone duration_ms");
            return false;
          }
          data->current_def.tone.duration_ms = d;
        }
        // Unknown tone sub-fields are ignored (forward compatibility).
      }
      // When parent is "sequence", stack_pos 8 is a note field key; its value
      // is handled in case 9.
      break;
    }

    // Sequence note field value (key at stack[8]).
    case 9: {
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
  // Master volume is runtime-adjustable by the HL/app via a service input.
  set_volume(new_value);
}
