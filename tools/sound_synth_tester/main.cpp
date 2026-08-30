/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file main.cpp
 * @brief Host-side synthesis tester — renders note sequences to raw PCM.
 *
 * Drives the exact same Synth used by the firmware (sound_synth.cpp) and streams
 * raw 16-bit signed little-endian mono PCM at 16 kHz to stdout, so a sequence can
 * be auditioned without flashing the robot:
 *
 *   ./sound_synth_tester "440:200 660:350" | aplay -f S16_LE -r 16000 -c 1
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "sound_definition.hpp"
#include "sound_synth.hpp"

using namespace xbot::driver::sound;

namespace {

constexpr size_t kChunkFrames = 256U;  ///< frames rendered per fill() call

void usage(const char* argv0) {
  fprintf(stderr,
          "Usage: %s \"freq:dur[:lfoHzx10[:lfoDepth]] [note2 ...]\" [--volume N] [--repeat N] [--waveform W] [--unison "
          "N] [--detune HZ]\n"
          "       %s --list\n"
          "\n"
          "  freq      fundamental frequency in Hz (0 = silence/pause)\n"
          "  dur       note duration in ms\n"
          "  lfoHzx10  LFO rate x10 (e.g. 20 = 2.0 Hz), default 0 = off\n"
          "  lfoDepth  LFO frequency deviation in Hz, default 0\n"
          "  --waveform W  oscillator waveform: sine|square|triangle|saw (default sine)\n"
          "  --unison N    stack N detuned voices (odd: 1/3/5/7, default 1)\n"
          "  --detune HZ   frequency spread between unison voices in Hz (default 0)\n"
          "\n"
          "Streams raw mono int16 PCM @ %u Hz to stdout (pipe into a player):\n"
          "  %s \"440:200 660:350\" | aplay -f S16_LE -r %u -c 1\n"
          "  %s \"950:8000:20:220\" --repeat 3 | aplay -f S16_LE -r %u -c 1\n"
          "\n"
          "Max %u notes per sequence (matches kMaxNotes).\n",
          argv0, argv0, SAMPLE_RATE, argv0, SAMPLE_RATE, argv0, SAMPLE_RATE, kMaxNotes);
}

/** @brief Parse one "freq:dur[:lfoHzx10[:lfoDepth]]" token into a Note. */
bool parse_note(const char* tok, Note& out) {
  uint32_t v[4] = {0, 0, 0, 0};
  int n = 0;
  const char* p = tok;
  while (n < 4) {
    char* end = nullptr;
    unsigned long x = strtoul(p, &end, 10);
    if (end == p) break; /* no number parsed */
    v[n++] = static_cast<uint32_t>(x);
    if (*end != ':') break;
    p = end + 1;
  }
  if (n < 2) return false;
  out = {static_cast<uint16_t>(v[0]), static_cast<uint16_t>(v[1]), static_cast<uint16_t>(v[2]),
         static_cast<uint16_t>(v[3])};
  return true;
}

/** @brief Parse a waveform name. */
bool parse_waveform(const char* name, Waveform& out) {
  if (strcmp(name, "sine") == 0) {
    out = Waveform::SINE;
    return true;
  }
  if (strcmp(name, "square") == 0) {
    out = Waveform::SQUARE;
    return true;
  }
  if (strcmp(name, "triangle") == 0) {
    out = Waveform::TRIANGLE;
    return true;
  }
  if (strcmp(name, "saw") == 0) {
    out = Waveform::SAW;
    return true;
  }
  return false;
}

void render(const Note* notes, uint8_t count, uint8_t volume, int repeat, Waveform waveform, uint8_t unison,
            uint32_t detune_hz) {
  int16_t stereo[2 * kChunkFrames];
  int16_t mono[kChunkFrames];
  for (int r = 0; r < repeat; ++r) {
    Synth synth;
    synth.set_unison(unison, detune_hz);
    synth.start_sequence(notes, count, waveform);
    while (true) {
      const bool active = synth.fill(stereo, kChunkFrames, volume);
      for (size_t i = 0; i < kChunkFrames; ++i) {
        mono[i] = stereo[2U * i]; /* left channel (R is always silent) */
      }
      fwrite(mono, sizeof(int16_t), kChunkFrames, stdout);
      if (!active) break;
    }
  }
  fflush(stdout);
}

static const char* waveform_name(Waveform w) {
  switch (w) {
    case Waveform::SQUARE: return "square";
    case Waveform::TRIANGLE: return "triangle";
    case Waveform::SAW: return "saw";
    default: return "sine";
  }
}

void list_defaults() {
  static const char* kNames[] = {"BOOT",  "BOOT_PING", "BOOT_COMPLETE", "SUCCESS",        "WARNING",
                                 "ERROR", "EMERGENCY", "LOW_BATTERY",   "CHARGING_START", "CHARGING_DONE"};
  printf("Default sequences (copy & tweak):\n");
  for (size_t i = 0; i < static_cast<size_t>(SoundId::COUNT); ++i) {
    const SoundDefinition& def = kDefaultSoundDefs[i];
    printf("  %-14s ", kNames[i]);
    if (def.type == SoundType::TONE) {
      printf("%u:%u", def.tone.freq, def.tone.duration_ms);
    } else {
      for (uint8_t j = 0; j < def.sequence.count; ++j) {
        const Note& n = def.sequence.notes[j];
        if (n.lfo_hz_x10 != 0U || n.lfo_depth != 0U) {
          printf("%u:%u:%u:%u ", n.freq, n.duration_ms, n.lfo_hz_x10, n.lfo_depth);
        } else {
          printf("%u:%u ", n.freq, n.duration_ms);
        }
      }
    }
    if (def.waveform != Waveform::SINE || def.unison != 1U || def.detune_hz != 0U) {
      printf(" [%s, unison %u, detune %uHz]", waveform_name(def.waveform), def.unison, def.detune_hz);
    }
    printf("\n");
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    usage(argv[0]);
    return 2;
  }

  uint8_t volume = 80;
  int repeat = 1;
  Waveform waveform = Waveform::SINE;
  uint8_t unison = 1;
  uint32_t detune_hz = 0;
  const char* seq = nullptr;

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      usage(argv[0]);
      return 0;
    } else if (strcmp(argv[i], "--list") == 0) {
      list_defaults();
      return 0;
    } else if (strcmp(argv[i], "--volume") == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        return 2;
      }
      volume = static_cast<uint8_t>(strtoul(argv[++i], nullptr, 10));
      if (volume > 100U) volume = 100U;
    } else if (strcmp(argv[i], "--repeat") == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        return 2;
      }
      repeat = atoi(argv[++i]);
      if (repeat < 1) repeat = 1;
    } else if (strcmp(argv[i], "--waveform") == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        return 2;
      }
      if (!parse_waveform(argv[++i], waveform)) {
        fprintf(stderr, "unknown waveform: %s (sine|square|triangle|saw)\n", argv[i]);
        return 2;
      }
    } else if (strcmp(argv[i], "--unison") == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        return 2;
      }
      unison = static_cast<uint8_t>(strtoul(argv[++i], nullptr, 10));
    } else if (strcmp(argv[i], "--detune") == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        return 2;
      }
      detune_hz = static_cast<uint32_t>(strtoul(argv[++i], nullptr, 10));
    } else if (argv[i][0] == '-') {
      fprintf(stderr, "unknown option: %s\n", argv[i]);
      usage(argv[0]);
      return 2;
    } else if (seq == nullptr) {
      seq = argv[i];
    } else {
      fprintf(stderr, "unexpected argument: %s\n", argv[i]);
      usage(argv[0]);
      return 2;
    }
  }

  if (seq == nullptr) {
    usage(argv[0]);
    return 2;
  }

  std::vector<Note> notes;
  {
    const char* p = seq;
    while (*p) {
      while (*p == ' ') ++p; /* skip spaces */
      if (*p == '\0') break;
      const char* start = p;
      while (*p && *p != ' ') ++p;
      const std::string tok(start, p - start);
      Note note{};
      if (!parse_note(tok.c_str(), note)) {
        fprintf(stderr, "bad note: '%s' (expected freq:dur[:lfoHzx10[:lfoDepth]])\n", tok.c_str());
        usage(argv[0]);
        return 2;
      }
      notes.push_back(note);
    }
  }

  if (notes.empty()) {
    usage(argv[0]);
    return 2;
  }
  if (notes.size() > kMaxNotes) {
    fprintf(stderr, "warning: %zu notes given, clamping to %u (kMaxNotes)\n", notes.size(), kMaxNotes);
    notes.resize(kMaxNotes);
  }

  render(notes.data(), static_cast<uint8_t>(notes.size()), volume, repeat, waveform, unison, detune_hz);
  return 0;
}
