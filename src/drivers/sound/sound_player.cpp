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

#include <ulog.h>

#include <cstdio>
#include <cstring>

#include "filesystem/file.hpp"
#include "filesystem/filesystem.hpp"
#include "hal.h"
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
/* Sound request type.                                                       */
/*===========================================================================*/

struct SoundRequest {
  enum class Type : uint8_t { TONE, FILE } type;
  union {
    struct {
      uint32_t freq;
      uint32_t duration_ms;
      uint8_t volume;
    } tone;
    char path[64];
  };
};

/*===========================================================================*/
/* Active source state (owned by player thread).                             */
/*===========================================================================*/

enum class SourceType : uint8_t { NONE, TONE, WAV };

struct ActiveSource {
  SourceType type = SourceType::NONE;
  /* TONE fields */
  uint32_t phase = 0U;
  uint32_t phase_inc = 0U;
  uint32_t samples_left = 0U;
  /* WAV fields */
  File wav_file;
  /* Common */
  uint8_t volume = 80U;

  bool is_active() const {
    return type != SourceType::NONE;
  }

  void stop() {
    if (type == SourceType::WAV) {
      wav_file.close();
    }
    type = SourceType::NONE;
  }
};

/*===========================================================================*/
/* Thread and mailbox storage.                                               */
/*===========================================================================*/

static constexpr eventmask_t EVT_HTIF = EVENT_MASK(0U);     ///< DMA finished first half
static constexpr eventmask_t EVT_TCIF = EVENT_MASK(1U);     ///< DMA finished second half
static constexpr eventmask_t EVT_REQUEST = EVENT_MASK(2U);  ///< New request enqueued
static constexpr eventmask_t EVT_STOP = EVENT_MASK(3U);     ///< Terminate thread

static THD_WORKING_AREA(s_player_wa, 2048U);
static thread_t* s_player_thd = nullptr;

/* HIGH priority queue: depth 1, single storage slot */
static SoundRequest s_high_req;
static msg_t s_high_mb_buf[1];
static mailbox_t s_high_mb;

/* NORMAL priority queue: depth 4, ring-buffer storage pool */
static SoundRequest s_normal_pool[4];
static uint8_t s_normal_pool_idx = 0U;
static msg_t s_normal_mb_buf[4];
static mailbox_t s_normal_mb;

/* Global volume (written by set_volume, read by start_source) */
static uint8_t s_volume = 80U;

/* Playing flag — written only by player thread, read by is_playing() */
static bool s_playing = false;

/* Active playback source — owned by player thread */
static ActiveSource s_source;

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

/*===========================================================================*/
/* Internal: fill one DMA half-buffer from the active source.               */
/*===========================================================================*/

/**
 * @brief Fill @p count int16_t slots (L+R stereo pairs) from the active source.
 *
 * Right channel is always written as 0 (MAX98357A is left-channel only).
 * When the source is exhausted mid-half the remainder is filled with silence
 * and @p s_source.type is set to @p SourceType::NONE.
 */
static void fill_half(int16_t* buf, size_t count) {
  const size_t frames = count / 2U; /* one frame = [L, R=0] */

  if (s_source.type == SourceType::TONE) {
    for (size_t i = 0U; i < frames; ++i) {
      int16_t s = 0;
      if (s_source.samples_left > 0U) {
        s = scale_volume(sine_sample(s_source.phase), s_source.volume);
        s_source.phase += s_source.phase_inc;
        if (--s_source.samples_left == 0U) {
          s_source.type = SourceType::NONE;
        }
      }
      buf[2U * i] = s;
      buf[2U * i + 1] = 0; /* R — always silent */
    }

  } else if (s_source.type == SourceType::WAV) {
    for (size_t i = 0U; i < frames; ++i) {
      int16_t s = 0;
      if (s_source.samples_left > 0U) {
        int16_t raw = 0;
        if (s_source.wav_file.read(&raw, sizeof(raw)) == static_cast<int>(sizeof(raw))) {
          s = scale_volume(raw, s_source.volume);
          if (--s_source.samples_left == 0U) {
            s_source.wav_file.close();
            s_source.type = SourceType::NONE;
          }
        } else {
          /* Read error — silence and stop */
          s_source.wav_file.close();
          s_source.type = SourceType::NONE;
        }
      }
      buf[2U * i] = s;
      buf[2U * i + 1] = 0;
    }

  } else {
    memset(buf, 0, count * sizeof(int16_t));
  }
}

/*===========================================================================*/
/* Internal: start playing a SoundRequest.                                  */
/*===========================================================================*/

static void start_source(const SoundRequest& req) {
  s_source.stop(); /* close any open WAV file */

  if (req.type == SoundRequest::Type::TONE) {
    s_source.type = SourceType::TONE;
    s_source.phase = 0U;
    s_source.phase_inc = calc_phase_increment(req.tone.freq);
    s_source.samples_left = (SAMPLE_RATE * req.tone.duration_ms) / 1000U;
    s_source.volume = req.tone.volume;

  } else {
    if (s_source.wav_file.open(req.path, LFS_O_RDONLY) != LFS_ERR_OK) {
      ULOG_WARNING("Sound: cannot open '%s'", req.path);
      return;
    }
    WavInfo info;
    if (!wav_parse_header(s_source.wav_file, info)) {
      ULOG_WARNING("Sound: invalid WAV '%s'", req.path);
      s_source.wav_file.close();
      return;
    }
    s_source.type = SourceType::WAV;
    s_source.samples_left = info.num_samples;
    s_source.volume = s_volume;
  }

  /* Pre-fill both halves before starting BDMA so DMA has valid data immediately. */
  fill_half(s_audio_buf, SOUND_HALF_SIZE);
  fill_half(s_audio_buf + SOUND_HALF_SIZE, SOUND_HALF_SIZE);

  i2sStartExchange(&I2SD6);
  s_playing = true;
}

/*===========================================================================*/
/* Internal: dequeue and play the next pending request (if any).            */
/*===========================================================================*/

static void dequeue_and_play() {
  if (s_playing) return;

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
    if (s_playing) {
      i2sStopExchange(&I2SD6);
      s_playing = false;
      s_source.stop();
    }
    start_source(s_high_req);
    return;
  }

  /* NORMAL: only start if idle */
  if (!s_playing) {
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
    const eventmask_t ev = chEvtWaitAny(EVT_HTIF | EVT_TCIF | EVT_REQUEST | EVT_STOP);

    if (ev & EVT_STOP) {
      break;
    }

    if (ev & EVT_REQUEST) {
      handle_request();
    }

    if (ev & EVT_HTIF) {
      /* DMA finished first half → refill first half */
      fill_half(s_audio_buf, SOUND_HALF_SIZE);
      if (!s_source.is_active() && s_playing) {
        i2sStopExchange(&I2SD6);
        s_playing = false;
        dequeue_and_play();
      }
    }

    if (ev & EVT_TCIF) {
      /* DMA finished second half → refill second half */
      fill_half(s_audio_buf + SOUND_HALF_SIZE, SOUND_HALF_SIZE);
      if (!s_source.is_active() && s_playing) {
        i2sStopExchange(&I2SD6);
        s_playing = false;
        dequeue_and_play();
      }
    }
  }
}

/*===========================================================================*/
/* Internal: enqueue helpers (called from any thread context).              */
/*===========================================================================*/

static void enqueue_high(const SoundRequest& req) {
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

static void enqueue_normal(const SoundRequest& req) {
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

  ULOG_INFO("Sound: player started (sample_rate=%u, volume=%u)", SAMPLE_RATE, s_volume);
}

void play_sound_id(SoundId id, bool high_priority) {
  const uint8_t idx = static_cast<uint8_t>(id);
  if (idx >= static_cast<uint8_t>(SoundId::COUNT)) return;

  /* Prefer WAV file from flash; fall back to synthesised tone if absent. */
  char path[32];
  snprintf(path, sizeof(path), "/sounds/%u.wav", static_cast<unsigned>(idx));

  lfs_info info{};
  if (lfs_stat(&lfs, path, &info) == LFS_ERR_OK) {
    play_file(path, high_priority);
  } else {
    const SoundFallback& fb = kFallbackTones[idx];
    play_tone(fb.freq, fb.duration_ms, fb.volume, high_priority);
  }
}

void play_tone(uint32_t freq, uint32_t duration_ms, uint8_t volume, bool high_priority) {
  if (freq == 0U || duration_ms == 0U) return;

  SoundRequest req;
  req.type = SoundRequest::Type::TONE;
  req.tone.freq = freq;
  req.tone.duration_ms = duration_ms;
  req.tone.volume = volume;

  if (high_priority) {
    enqueue_high(req);
  } else {
    enqueue_normal(req);
  }
}

void play_file(const char* path, bool high_priority) {
  if (path == nullptr) return;

  SoundRequest req;
  req.type = SoundRequest::Type::FILE;
  strncpy(req.path, path, sizeof(req.path) - 1U);
  req.path[sizeof(req.path) - 1U] = '\0';

  if (high_priority) {
    enqueue_high(req);
  } else {
    enqueue_normal(req);
  }
}

void set_volume(uint8_t volume) {
  if (volume > 100U) volume = 100U;
  s_volume = volume;
}

bool is_playing() {
  return s_playing;
}

}  // namespace xbot::driver::sound
