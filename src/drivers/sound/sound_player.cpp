/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sound_player.cpp
 * @brief Event-driven sound player for STM32H723 with MAX98357A.
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-03-23
 *
 * @note  Architecture overview:
 *
 *          Other code
 *            │ play_sound_id() / play_tone() / play_file()
 *            ▼
 *        ┌──────────────────────────────────────────┐
 *        │  player_thread  (NORMALPRIO + 1)          │
 *        │                                           │
 *        │  HIGH mailbox  (depth 1) ← preempts       │
 *        │  NORMAL mailbox (depth 4) ← FIFO queue    │
 *        │                                           │
 *        │  Owns: s_audio_buf in SRAM4, I2SD6        │
 *        │  ISR: chEvtSignalI(EVT_HTIF / EVT_TCIF)   │
 *        │  Fill: buf[i]=sample, buf[i+1]=0 (R=0)    │
 *        │  Sources: ToneSource | WavSource           │
 *        └──────────────────┬───────────────────────┘
 *                           │ BDMA circular, SRAM4
 *                           ▼
 *                    I2S6 → MAX98357A (left channel only)
 *
 *        Half-buffer fill order (DMA circular, 1024 samples total):
 *          HTIF (half-transfer): DMA finished first half  → refill first half
 *          TCIF (full-transfer): DMA finished second half → refill second half
 */

#include "sound_player.hpp"

#include <etl/atomic.h>
#include <ulog.h>

#include <cstdio>
#include <cstring>

#include "filesystem/file.hpp"
#include "filesystem/filesystem.hpp"
#include "hal.h"
#include "sound_definition.hpp"
#include "sound_ids.hpp"
#include "sound_wav.hpp"

namespace xbot::driver::sound {

/*===========================================================================*/
/* Constants.                                                                */
/*===========================================================================*/

/** Total int16_t elements in the double-buffer (L+R interleaved, two DMA halves). */
static constexpr size_t SOUND_BUFFER_SIZE = 1024U;
/** Elements per DMA half (512 = 256 stereo frames = 16 ms at 16 kHz). */
static constexpr size_t SOUND_HALF_SIZE = SOUND_BUFFER_SIZE / 2U;

static constexpr uint32_t SAMPLE_RATE = 16000U;

/* WAV files are pre-mastered at full scale; only the master volume scales them. */
static constexpr uint8_t WAV_VOLUME = 100U;

/*===========================================================================*/
/* DMA buffer — MUST reside in SRAM4 (D3 domain) for BDMA.                  */
/*===========================================================================*/

static int16_t s_audio_buf[SOUND_BUFFER_SIZE] __attribute__((section(".sram4")));

/*===========================================================================*/
/* Sine synthesis (folded from the deleted sound_samples.hpp).               */
/*===========================================================================*/

/**
 * @brief 64-entry sine lookup table.  One full period, values –32767…+32767.
 * Resides entirely in .rodata — 128 bytes flash, zero RAM.
 */
static constexpr int16_t kSineTable[64] = {
    0,      3212,   6393,   9512,   12540,  15446,  18205,  20787,  23170,  25330,  27246,  28899,  30273,
    31357,  32138,  32610,  32767,  32610,  32138,  31357,  30273,  28899,  27246,  25330,  23170,  20787,
    18205,  15446,  12540,  9512,   6393,   3212,   0,      -3212,  -6393,  -9512,  -12540, -15446, -18205,
    -20787, -23170, -25330, -27246, -28899, -30273, -31357, -32138, -32610, -32767, -32610, -32138, -31357,
    -30273, -28899, -27246, -25330, -23170, -20787, -18205, -15446, -12540, -9512,  -6393,  -3212,
};

/** @brief 16.16 fixed-point phase → sine sample. */
static inline int16_t sine_sample(uint32_t phase) {
  return kSineTable[(phase >> 16U) & 0x3FU];
}

/** @brief Compute 16.16 fixed-point phase increment for @p freq at SAMPLE_RATE. */
static inline uint32_t calc_phase_increment(uint32_t freq) {
  return static_cast<uint32_t>((static_cast<uint64_t>(freq) * 64U * 65536U) / SAMPLE_RATE);
}

/** @brief Scale @p sample by @p volume (0–100), clamped to int16_t range. */
static inline int16_t scale_volume(int16_t sample, uint8_t volume) {
  const int32_t v = static_cast<int32_t>(sample) * volume / 100;
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return static_cast<int16_t>(v);
}

/*===========================================================================*/
/* Active playback source — owns all synthesis/streaming state.              */
/*===========================================================================*/

/**
 * @brief Value-type sound source that fills the DMA half-buffer with samples.
 *
 * Exactly one source is active at a time (the file-static @p s_source).  It is
 * a plain struct without virtual dispatch, so it costs nothing at run time:
 * the type switch in fill() runs once per half-buffer, outside the sample
 * loop.  Add a format (e.g. a future compressed codec) by extending Type,
 * start_*() and fill_*().
 */
struct SoundSource {
  enum class Type : uint8_t { NONE, TONE, SEQUENCE, WAV };

  Type type = Type::NONE;

  /* TONE / SEQUENCE oscillator fields */
  uint32_t phase = 0U;
  uint32_t phase_inc = 0U;     ///< Base phase increment for current note
  uint32_t samples_left = 0U;  ///< Samples remaining in current note (or tone)
  /* SEQUENCE-specific fields */
  Note seq_notes[kMaxNotes]{};
  uint8_t seq_count = 0U;
  uint8_t seq_idx = 0U;
  /* LFO (SEQUENCE only; all zero when inactive) */
  uint32_t lfo_phase = 0U;
  uint32_t lfo_inc = 0U;
  int32_t lfo_depth_inc = 0;  ///< Modulation depth in phase_inc units (pre-calculated)
  /* WAV fields */
  File wav_file;
  /* Per-type volume (0-100). Combined with the master volume in fill(). */
  uint8_t volume = 80U;

  bool is_active() const {
    return type != Type::NONE;
  }

  /** @brief Release resources (close WAV file) and mark the source inactive. */
  void stop() {
    if (type == Type::WAV) {
      wav_file.close();
    }
    type = Type::NONE;
  }

  /** @brief Start playback from a @p SoundDefinition. @return false on error (source left inactive). */
  bool start(const SoundDefinition& def);
  /** @brief Fill @p count int16_t slots (L+R stereo pairs) from this source. */
  void fill(int16_t* buf, size_t count);

 private:
  void fill_tone(int16_t* buf, size_t frames, uint8_t vol);
  void fill_sequence(int16_t* buf, size_t frames, uint8_t vol);
  void fill_wav(int16_t* buf, size_t frames, uint8_t vol);
};

/*===========================================================================*/
/* Thread and mailbox storage.                                               */
/*===========================================================================*/

static constexpr eventmask_t EVT_HTIF = EVENT_MASK(0U);           ///< DMA finished first half
static constexpr eventmask_t EVT_TCIF = EVENT_MASK(1U);           ///< DMA finished second half
static constexpr eventmask_t EVT_REQUEST = EVENT_MASK(2U);        ///< New request enqueued
static constexpr eventmask_t EVT_STOP_PLAYBACK = EVENT_MASK(3U);  ///< Stop playback + flush queues

static THD_WORKING_AREA(s_player_wa, 2048U);
static thread_t* s_player_thd = nullptr;

/* HIGH priority queue: depth 1, single storage slot */
static SoundDefinition s_high_req;
static msg_t s_high_mb_buf[1];
static mailbox_t s_high_mb;

/* NORMAL priority queue: depth 4, ring-buffer storage pool */
static SoundDefinition s_normal_pool[4];
static uint8_t s_normal_pool_idx = 0U;
static msg_t s_normal_mb_buf[4];
static mailbox_t s_normal_mb;

/* Master volume (0-100, written by set_volume from any thread, read by the player thread) */
static etl::atomic<uint8_t> s_master_volume{100U};

/* Playing flag — written by the player thread, read by is_playing() from any thread */
static etl::atomic<bool> s_playing{false};

/* Active playback source — owned by player thread */
static SoundSource s_source;

/*===========================================================================*/
/* I2S configuration.                                                        */
/*===========================================================================*/

static void i2s_end_cb(I2SDriver* i2sp);

static I2SConfig s_i2s_cfg = {
    .tx_buffer = s_audio_buf,
    .rx_buffer = nullptr,
    .size = SOUND_BUFFER_SIZE,
    .end_cb = i2s_end_cb,
    .sample_rate = SAMPLE_RATE,
    .i2scfgr = 0U, /* Philips standard, DATLEN=16-bit, CHLEN=16-bit */
};

/*---------------------------------------------------------------------------*/
/* SoundSource method implementations.                                       */
/*---------------------------------------------------------------------------*/

bool SoundSource::start(const SoundDefinition& def) {
  switch (def.type) {
    case SoundType::TONE:
      type = Type::TONE;
      phase = 0U;
      phase_inc = calc_phase_increment(def.tone.freq);
      samples_left = (SAMPLE_RATE * def.tone.duration_ms) / 1000U;
      volume = def.volume;
      return true;

    case SoundType::SEQUENCE: {
      type = Type::SEQUENCE;
      seq_count = (def.sequence.count <= kMaxNotes) ? def.sequence.count : kMaxNotes;
      for (uint8_t i = 0U; i < seq_count; ++i) {
        seq_notes[i] = def.sequence.notes[i];
      }
      seq_idx = 0U;
      volume = def.volume;
      /* Reset oscillator and LFO — first note is loaded by fill() on first call. */
      phase = 0U;
      phase_inc = 0U;
      samples_left = 0U;
      lfo_phase = 0U;
      lfo_inc = 0U;
      lfo_depth_inc = 0;
      return true;
    }

    case SoundType::FILE:
      if (wav_file.open(def.path, LFS_O_RDONLY) != LFS_ERR_OK) {
        ULOG_WARNING("Sound: cannot open '%s'", def.path);
        return false;
      }
      WavInfo info;
      if (!wav_parse_header(wav_file, info)) {
        ULOG_WARNING("Sound: invalid WAV '%s'", def.path);
        wav_file.close();
        return false;
      }
      type = Type::WAV;
      samples_left = info.num_samples;
      volume = def.volume;
      return true;

    default: return false;
  }
}

/**
 * @brief Fill @p count int16_t slots (L+R stereo pairs) from this source.
 *
 * Right channel is always written as 0 (MAX98357A is left-channel only).
 * When the source is exhausted mid-half the remainder is filled with silence
 * and @p type is set to @p Type::NONE.
 */
void SoundSource::fill(int16_t* buf, size_t count) {
  const size_t frames = count / 2U; /* one frame = [L, R=0] */
  /* Combine the per-type volume with the master volume into a single 0-100
     scale so only one division is needed per sample. */
  const uint8_t vol = static_cast<uint8_t>((static_cast<uint32_t>(volume) * s_master_volume.load()) / 100U);
  switch (type) {
    case Type::TONE: fill_tone(buf, frames, vol); break;
    case Type::SEQUENCE: fill_sequence(buf, frames, vol); break;
    case Type::WAV: fill_wav(buf, frames, vol); break;
    default: memset(buf, 0, count * sizeof(int16_t)); break;
  }
}

void SoundSource::fill_tone(int16_t* buf, size_t frames, uint8_t vol) {
  for (size_t i = 0U; i < frames; ++i) {
    int16_t s = 0;
    if (samples_left > 0U) {
      s = scale_volume(sine_sample(phase), vol);
      phase += phase_inc;
      if (--samples_left == 0U) {
        type = Type::NONE;
      }
    }
    buf[2U * i] = s;
    buf[2U * i + 1] = 0; /* R — always silent */
  }
}

void SoundSource::fill_sequence(int16_t* buf, size_t frames, uint8_t vol) {
  for (size_t i = 0U; i < frames; ++i) {
    /* Advance to the next note whenever the current one has been exhausted. */
    while (samples_left == 0U && seq_idx < seq_count) {
      const Note& n = seq_notes[seq_idx++];
      samples_left = (SAMPLE_RATE * n.duration_ms) / 1000U;
      phase = 0U;
      phase_inc = (n.freq > 0U) ? calc_phase_increment(n.freq) : 0U;
      if (n.lfo_hz_x10 > 0U && n.freq > 0U) {
        lfo_phase = 0U;
        /* LFO phase increment: same formula as calc_phase_increment but divided by 10
           to convert lfo_hz_x10 (rate × 10) back to Hz. */
        lfo_inc = static_cast<uint32_t>(static_cast<uint64_t>(n.lfo_hz_x10) * 64U * 65536U / (SAMPLE_RATE * 10U));
        /* Depth in phase_inc units — pre-calculated to avoid per-sample division. */
        lfo_depth_inc = static_cast<int32_t>(calc_phase_increment(n.freq + n.lfo_depth) - phase_inc);
      } else {
        lfo_inc = 0U;
        lfo_depth_inc = 0;
      }
    }

    int16_t s = 0;
    if (samples_left > 0U) {
      if (phase_inc > 0U) {
        /* Apply LFO frequency modulation (no-op when lfo_inc == 0). */
        uint32_t eff_inc = phase_inc;
        if (lfo_inc > 0U) {
          eff_inc += static_cast<uint32_t>((static_cast<int64_t>(lfo_depth_inc) * sine_sample(lfo_phase)) >> 15);
          lfo_phase += lfo_inc;
        }
        s = scale_volume(sine_sample(phase), vol);
        phase += eff_inc;
      }
      if (--samples_left == 0U && seq_idx >= seq_count) {
        type = Type::NONE; /* sequence finished */
      }
    }
    buf[2U * i] = s;
    buf[2U * i + 1] = 0;
  }
}

void SoundSource::fill_wav(int16_t* buf, size_t frames, uint8_t vol) {
  for (size_t i = 0U; i < frames; ++i) {
    int16_t s = 0;
    if (samples_left > 0U) {
      int16_t raw = 0;
      if (wav_file.read(&raw, sizeof(raw)) == static_cast<int>(sizeof(raw))) {
        s = scale_volume(raw, vol);
        if (--samples_left == 0U) {
          wav_file.close();
          type = Type::NONE;
        }
      } else {
        /* Read error — silence and stop */
        wav_file.close();
        type = Type::NONE;
      }
    }
    buf[2U * i] = s;
    buf[2U * i + 1] = 0;
  }
}

/*===========================================================================*/
/* Internal: start playing a SoundDefinition.                               */
/*===========================================================================*/

static void start_source(const SoundDefinition& def) {
  s_source.stop(); /* close any open WAV file */

  if (!s_source.start(def)) {
    return; /* FILE open/parse failed — remain idle */
  }

  /* Pre-fill both halves before starting BDMA so DMA has valid data immediately. */
  s_source.fill(s_audio_buf, SOUND_HALF_SIZE);
  s_source.fill(s_audio_buf + SOUND_HALF_SIZE, SOUND_HALF_SIZE);

  i2sStartExchange(&I2SD6);
  s_playing.store(true);
}

/*===========================================================================*/
/* Internal: dequeue and play the next pending request (if any).            */
/*===========================================================================*/

static void dequeue_and_play() {
  if (s_playing.load()) return;

  msg_t idx;
  /* HIGH has priority even in the normal dequeue path */
  if (chMBFetchTimeout(&s_high_mb, &idx, TIME_IMMEDIATE) == MSG_OK) {
    start_source(s_high_req);
    return;
  }
  if (chMBFetchTimeout(&s_normal_mb, &idx, TIME_IMMEDIATE) == MSG_OK) {
    start_source(s_normal_pool[static_cast<uint8_t>(idx)]);
  }
}

/*===========================================================================*/
/* Internal: handle a newly arrived EVT_REQUEST.                            */
/*===========================================================================*/

static void handle_request() {
  msg_t idx;

  /* HIGH always preempts current playback */
  if (chMBFetchTimeout(&s_high_mb, &idx, TIME_IMMEDIATE) == MSG_OK) {
    if (s_playing.load()) {
      i2sStopExchange(&I2SD6);
      s_playing.store(false);
      s_source.stop();
    }
    start_source(s_high_req);
    return;
  }

  /* NORMAL: only start if idle */
  if (!s_playing.load()) {
    if (chMBFetchTimeout(&s_normal_mb, &idx, TIME_IMMEDIATE) == MSG_OK) {
      start_source(s_normal_pool[static_cast<uint8_t>(idx)]);
    }
  }
}

/*===========================================================================*/
/* ISR callback — minimal: signals the player thread with the correct event.*/
/*===========================================================================*/

/**
 * @note  Half-buffer semantics (DMA circular):
 *          i2sIsBufferComplete() == false  → HTIF: DMA finished first half,
 *                                            now at second  → player refills first half.
 *          i2sIsBufferComplete() == true   → TCIF: DMA finished second half,
 *                                            wrapping back  → player refills second half.
 */
static void i2s_end_cb(I2SDriver* i2sp) {
  chSysLockFromISR();
  if (i2sIsBufferComplete(i2sp)) {
    chEvtSignalI(s_player_thd, EVT_TCIF); /* refill second half */
  } else {
    chEvtSignalI(s_player_thd, EVT_HTIF); /* refill first half */
  }
  chSysUnlockFromISR();
}

/*===========================================================================*/
/* Player thread.                                                            */
/*===========================================================================*/

static THD_FUNCTION(player_thread, arg) {
  (void)arg;
  chRegSetThreadName("sound");

  while (true) {
    const eventmask_t ev = chEvtWaitAny(EVT_HTIF | EVT_TCIF | EVT_REQUEST | EVT_STOP_PLAYBACK);

    if (ev & EVT_STOP_PLAYBACK) {
      /* Stop current playback; the queues have already been flushed by stop(). */
      if (s_playing.load()) {
        i2sStopExchange(&I2SD6);
        s_playing.store(false);
        s_source.stop();
      }
    }

    if (ev & EVT_REQUEST) {
      handle_request();
    }

    if (ev & EVT_HTIF) {
      /* DMA finished first half → refill first half */
      s_source.fill(s_audio_buf, SOUND_HALF_SIZE);
      if (!s_source.is_active() && s_playing.load()) {
        i2sStopExchange(&I2SD6);
        s_playing.store(false);
        dequeue_and_play();
      }
    }

    if (ev & EVT_TCIF) {
      /* DMA finished second half → refill second half */
      s_source.fill(s_audio_buf + SOUND_HALF_SIZE, SOUND_HALF_SIZE);
      if (!s_source.is_active() && s_playing.load()) {
        i2sStopExchange(&I2SD6);
        s_playing.store(false);
        dequeue_and_play();
      }
    }
  }
}

/*===========================================================================*/
/* Internal: enqueue helpers (called from any thread context).              */
/*===========================================================================*/

static void enqueue_high(const SoundDefinition& req) {
  if (s_player_thd == nullptr) return;
  chSysLock();
  s_high_req = req;         /* replace any pending high request */
  chMBResetI(&s_high_mb);   /* flush stale entry (if any) */
  chMBPostI(&s_high_mb, 0); /* always succeeds after reset */
  chEvtSignalI(s_player_thd, EVT_REQUEST);
  /* Required after I-class calls that may have made a higher-priority thread
     ready: chSysUnlock() asserts "priority order violation" if we skip this. */
  chSchRescheduleS();
  chSysUnlock();
}

static void enqueue_normal(const SoundDefinition& req) {
  if (s_player_thd == nullptr) return;
  chSysLock();
  if (chMBGetFreeCountI(&s_normal_mb) > 0) {
    const uint8_t idx = s_normal_pool_idx;
    s_normal_pool_idx = (s_normal_pool_idx + 1U) & 3U;
    s_normal_pool[idx] = req;
    chMBPostI(&s_normal_mb, idx);
  }
  /* Signal even if dropped — player will find nothing and stay idle (harmless). */
  chEvtSignalI(s_player_thd, EVT_REQUEST);
  /* Required after I-class calls that may have made a higher-priority thread
     ready: chSysUnlock() asserts "priority order violation" if we skip this. */
  chSchRescheduleS();
  chSysUnlock();
}

/*===========================================================================*/
/* Public API.                                                               */
/*===========================================================================*/

void player_init() {
  if (s_player_thd != nullptr) return; /* idempotent */

  chMBObjectInit(&s_high_mb, s_high_mb_buf, 1);
  chMBObjectInit(&s_normal_mb, s_normal_mb_buf, 4);

  i2sStart(&I2SD6, &s_i2s_cfg);

  s_player_thd = chThdCreateStatic(s_player_wa, sizeof(s_player_wa), NORMALPRIO + 1, player_thread, nullptr);

  ULOG_INFO("Sound: player started (sample_rate=%u, volume=%u)", SAMPLE_RATE, s_master_volume.load());
}

/**
 * @brief Load a flash override for @p id, if present.
 *
 * TODO: read the raw @p SoundDefinition from LittleFS (written by the
 * high-level system or a future Sound-CLI).  Until then this always returns
 * false so the ROM default applies.
 */
static bool load_sound_definition(SoundId id, SoundDefinition& out) {
  (void)id;
  (void)out;
  return false;
}

void play_sound_id(SoundId id, bool high_priority) {
  const uint8_t idx = static_cast<uint8_t>(id);
  if (idx >= static_cast<uint8_t>(SoundId::COUNT)) return;

  /* Prefer a flash override; otherwise fall back to the ROM default. */
  SoundDefinition def;
  if (!load_sound_definition(id, def)) {
    def = kDefaultSoundDefs[idx];
  }

  if (high_priority) {
    enqueue_high(def);
  } else {
    enqueue_normal(def);
  }
}

void play_tone(uint32_t freq, uint32_t duration_ms, uint8_t volume, bool high_priority) {
  if (freq == 0U || duration_ms == 0U) return;

  SoundDefinition def{};
  def.type = SoundType::TONE;
  def.volume = volume;
  def.tone.freq = freq;
  def.tone.duration_ms = duration_ms;

  if (high_priority) {
    enqueue_high(def);
  } else {
    enqueue_normal(def);
  }
}

void play_file(const char* path, bool high_priority) {
  if (path == nullptr) return;

  SoundDefinition def{};
  def.type = SoundType::FILE;
  def.volume = WAV_VOLUME; /* pre-mastered at full scale; master volume scales it */
  strncpy(def.path, path, kMaxPath - 1U);
  def.path[kMaxPath - 1U] = '\0';

  if (high_priority) {
    enqueue_high(def);
  } else {
    enqueue_normal(def);
  }
}

void set_volume(uint8_t volume) {
  if (volume > 100U) volume = 100U;
  s_master_volume.store(volume);
}

void stop() {
  if (s_player_thd == nullptr) return;
  chSysLock();
  /* Flush pending requests so playback cannot resume after stop. */
  chMBResetI(&s_high_mb);
  chMBResetI(&s_normal_mb);
  chEvtSignalI(s_player_thd, EVT_STOP_PLAYBACK);
  /* Required after I-class calls that may have made a higher-priority thread
     ready: chSysUnlock() asserts "priority order violation" if we skip this. */
  chSchRescheduleS();
  chSysUnlock();
}

bool is_playing() {
  return s_playing.load();
}

}  // namespace xbot::driver::sound
