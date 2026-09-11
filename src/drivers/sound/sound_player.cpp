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

#include "filesystem/file.hpp"
#include "hal.h"
#include "sound_definition.hpp"
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

/* Persisted sound-definition overrides (LittleFS) — survive a reboot. */
static constexpr const char* kSoundDefsPath = "/cfg/sound_defs.bin";
static constexpr uint32_t kSoundDefsMagic = 0x53444632U;  // "SDF2"
static constexpr uint16_t kSoundDefsVersion = 1U;

/** On-disk layout of the persisted sound overrides. */
struct PersistedDefsHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t count;
  uint16_t valid_mask;
};

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

/**
 * @note  minimp3's mp3dec_decode_frame() alone needs ~16.6 KB of stack (measured
 *        with -fstack-usage; it is independent of the optimisation level, so
 *        -O2 does not help). It is called from this thread, on top of the DMA
 *        refill chain, so 16 KB was too small and tripped the stack guard page
 *        (total silence, no log). 24 KB leaves ~7 KB headroom.
 */
static THD_WORKING_AREA(s_player_wa, 24576U);
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

/* Runtime sound overrides (SoundId -> definition), written by the SoundService
   during configuration and read by load_sound_definition() from play_sound_id(). */
static MUTEX_DECL(s_override_mutex);
static SoundDefinition s_sound_overrides[SoundId_count];
static bool s_override_valid[SoundId_count];

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

  // Apply persisted overrides (if any) so early sounds already use the
  // last-known high-level definitions instead of the ROM defaults.
  load_sound_overrides_from_storage();

  ULOG_INFO("Sound: player started (sample_rate=%u, volume=%u)", SAMPLE_RATE, s_master_volume.load());
}

/**
 * @brief Load a runtime override for @p id, if present.
 *
 * The SoundService stores parsed overrides (from the HL configuration blob)
 * via set_sound_override(); otherwise the ROM default applies.
 */
static bool load_sound_definition(SoundId id, SoundDefinition& out) {
  const uint8_t idx = static_cast<uint8_t>(id);
  if (idx >= SoundId_count) return false;

  chMtxLock(&s_override_mutex);
  const bool has_override = s_override_valid[idx];
  if (has_override) {
    out = s_sound_overrides[idx];
  }
  chMtxUnlock(&s_override_mutex);
  return has_override;
}

void play_sound_id(SoundId id, bool high_priority) {
  if (s_player_thd == nullptr) return;
  const uint8_t idx = static_cast<uint8_t>(id);
  if (idx >= SoundId_count) return;

  /* Prefer a flash override; otherwise fall back to the ROM default. */
  SoundDefinition def;
  const bool has_override = load_sound_definition(id, def);
  if (!has_override) {
    def = kDefaultSoundDefs[idx];
  }
  // INFO (not DEBUG): the remote log is filtered at ULOG_INFO_LEVEL, and this
  // line is the "override vs. ROM default" evidence after a reboot.
  const char* const source = has_override ? "flash override" : "ROM default";
  if (def.type == SoundType::MP3) {
    ULOG_INFO("Sound: play %s from %s (type=MP3, file='%s', volume=%u)", SoundId_to_string(id), source, def.path,
              static_cast<unsigned>(def.volume));
  } else {
    ULOG_INFO("Sound: play %s from %s (type=%s, volume=%u)", SoundId_to_string(id), source,
              SoundType_to_string(def.type), static_cast<unsigned>(def.volume));
  }

  if (high_priority) {
    enqueue_high(def);
  } else {
    enqueue_normal(def);
  }
}

void play_tone(uint32_t freq, uint32_t duration_ms, uint8_t volume, bool high_priority) {
  if (s_player_thd == nullptr) return;
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
  if (s_player_thd == nullptr) return;
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

void set_sound_override(SoundId id, const SoundDefinition& def) {
  const uint8_t idx = static_cast<uint8_t>(id);
  if (idx >= SoundId_count) return;

  chMtxLock(&s_override_mutex);
  s_sound_overrides[idx] = def;
  s_override_valid[idx] = true;
  chMtxUnlock(&s_override_mutex);
}

void clear_sound_overrides() {
  chMtxLock(&s_override_mutex);
  for (uint8_t i = 0U; i < SoundId_count; ++i) {
    s_override_valid[i] = false;
  }
  chMtxUnlock(&s_override_mutex);
}

void load_sound_overrides_from_storage() {
  // Keep the File (~330 B: 256 B cache) and the definition array (~840 B) out of
  // the main thread stack, which is only ~2.3 KB here (see the linker script).
  static File file;
  static SoundDefinition defs[SoundId_count];

  file.close();  // no-op unless an earlier attempt left it open
  if (file.open(kSoundDefsPath, LFS_O_RDONLY) != LFS_ERR_OK) {
    ULOG_INFO("Sound: no persisted overrides ('%s' missing), using ROM defaults", kSoundDefsPath);
    return;  // No persisted overrides — fall back to ROM defaults.
  }

  PersistedDefsHeader h{};
  int n = file.read(&h, sizeof(h));
  if (n != static_cast<int>(sizeof(h)) || h.magic != kSoundDefsMagic || h.version != kSoundDefsVersion ||
      h.count != SoundId_count) {
    ULOG_WARNING("Sound: defs store invalid, ignoring");
    file.close();
    return;
  }

  n = file.read(defs, sizeof(defs));
  file.close();
  if (n != static_cast<int>(sizeof(defs))) {
    ULOG_WARNING("Sound: defs store truncated, ignoring");
    return;
  }

  uint8_t loaded = 0U;
  chMtxLock(&s_override_mutex);
  for (uint8_t i = 0U; i < SoundId_count; ++i) {
    if ((h.valid_mask & (1U << i)) != 0U) {
      s_sound_overrides[i] = defs[i];
      s_override_valid[i] = true;
      ++loaded;
    } else {
      s_override_valid[i] = false;
    }
  }
  chMtxUnlock(&s_override_mutex);

  ULOG_INFO("Sound: loaded %u override(s) from flash (mask=0x%04x)", static_cast<unsigned>(loaded),
            static_cast<unsigned>(h.valid_mask));
}

void save_sound_overrides_to_storage() {
  uint16_t valid_mask = 0U;
  uint8_t count = 0U;
  chMtxLock(&s_override_mutex);
  for (uint8_t i = 0U; i < SoundId_count; ++i) {
    if (s_override_valid[i]) {
      valid_mask |= static_cast<uint16_t>(1U << i);
      ++count;
    }
  }
  chMtxUnlock(&s_override_mutex);

  // A File is ~330 bytes (256 B cache buffer) and this runs from the
  // SoundService thread's configuration callback, whose working area is only
  // 3 KB — the JSON parser frames are still live at this point. Keep a single
  // static instance (mkdirp() does not touch the file handle) off the stack.
  static File file;
  file.close();  // no-op unless an earlier attempt left it open

  // Ensure the /cfg directory exists (idempotent).
  if (file.mkdirp(kSoundDefsPath) != LFS_ERR_OK) {
    ULOG_WARNING("Sound: cannot create defs store dir");
    return;
  }

  if (file.open(kSoundDefsPath, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC) != LFS_ERR_OK) {
    ULOG_WARNING("Sound: cannot open defs store '%s'", kSoundDefsPath);
    return;
  }

  PersistedDefsHeader h{};
  h.magic = kSoundDefsMagic;
  h.version = kSoundDefsVersion;
  h.count = SoundId_count;
  h.valid_mask = valid_mask;

  int written = file.write(&h, sizeof(h));
  if (written == static_cast<int>(sizeof(h))) {
    written = file.write(s_sound_overrides, sizeof(s_sound_overrides));
  }
  file.sync();
  file.close();

  if (written < 0) {
    ULOG_WARNING("Sound: defs store write failed");
  } else {
    ULOG_INFO("Sound: persisted %u override(s) mask=0x%04x (%d bytes) to %s", static_cast<unsigned>(count),
              static_cast<unsigned>(valid_mask), written, kSoundDefsPath);
  }
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
