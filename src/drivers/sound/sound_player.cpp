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

#include <cstring>

#include "hal.h"
#include "sound_definition.hpp"
#include "sound_id.hpp"
#include "sound_source.hpp"

namespace xbot::driver::sound {

/*===========================================================================*/
/* Constants.                                                                */
/*===========================================================================*/

/** Total int16_t elements in the double-buffer (L+R interleaved, two DMA halves). */
static constexpr size_t SOUND_BUFFER_SIZE = 1024U;
/** Elements per DMA half (512 = 256 stereo frames = 16 ms at 16 kHz). */
static constexpr size_t SOUND_HALF_SIZE = SOUND_BUFFER_SIZE / 2U;

/* MP3 sources are pre-mastered at full scale; only the master volume scales them. */
static constexpr uint8_t kFileVolume = 100U;

/*===========================================================================*/
/* DMA buffer — MUST reside in SRAM4 (D3 domain) for BDMA.                  */
/*===========================================================================*/

static int16_t s_audio_buf[SOUND_BUFFER_SIZE] __attribute__((section(".sram4")));

/*===========================================================================*/
/* Thread and mailbox storage.                                               */
/*===========================================================================*/

static constexpr eventmask_t EVT_HTIF = EVENT_MASK(0U);           ///< DMA finished first half
static constexpr eventmask_t EVT_TCIF = EVENT_MASK(1U);           ///< DMA finished second half
static constexpr eventmask_t EVT_REQUEST = EVENT_MASK(2U);        ///< New request enqueued
static constexpr eventmask_t EVT_STOP_PLAYBACK = EVENT_MASK(3U);  ///< Stop playback + flush queues

/* minimp3 needs a ~13 KB scratch buffer on the stack during mp3dec_decode_frame(). */
static THD_WORKING_AREA(s_player_wa, 16384U);
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

/*===========================================================================*/
/* Internal: start playing a SoundDefinition.                               */
/*===========================================================================*/

static void play_definition(const SoundDefinition& def) {
  s_source.stop(); /* close any open WAV file */

  if (!s_source.start(def)) {
    return; /* FILE open/parse failed — remain idle */
  }

  /* Pre-fill both halves before starting BDMA so DMA has valid data immediately. */
  s_source.fill(s_audio_buf, SOUND_HALF_SIZE, s_master_volume.load());
  s_source.fill(s_audio_buf + SOUND_HALF_SIZE, SOUND_HALF_SIZE, s_master_volume.load());

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
    play_definition(s_high_req);
    return;
  }
  if (chMBFetchTimeout(&s_normal_mb, &idx, TIME_IMMEDIATE) == MSG_OK) {
    play_definition(s_normal_pool[static_cast<uint8_t>(idx)]);
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
    play_definition(s_high_req);
    return;
  }

  /* NORMAL: only start if idle */
  if (!s_playing.load()) {
    if (chMBFetchTimeout(&s_normal_mb, &idx, TIME_IMMEDIATE) == MSG_OK) {
      play_definition(s_normal_pool[static_cast<uint8_t>(idx)]);
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
      s_source.fill(s_audio_buf, SOUND_HALF_SIZE, s_master_volume.load());
      if (!s_source.is_active() && s_playing.load()) {
        i2sStopExchange(&I2SD6);
        s_playing.store(false);
        dequeue_and_play();
      }
    }

    if (ev & EVT_TCIF) {
      /* DMA finished second half → refill second half */
      s_source.fill(s_audio_buf + SOUND_HALF_SIZE, SOUND_HALF_SIZE, s_master_volume.load());
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
  def.type = SoundType::MP3;
  def.volume = kFileVolume; /* pre-mastered at full scale; master volume scales it */
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
