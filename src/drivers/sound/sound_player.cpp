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
 *        ┌────────────────────────────────────────────┐
 *        │  player_thread  (NORMALPRIO + 1)           │
 *        │                                            │
 *        │  request queue (etl::queue, depth 4) ← FIFO│
 *        │  preempt request: stop + clear + play now  │
 *        │                                            │
 *        │  Owns: s_audio_buf in SRAM4, I2SD6         │
 *        │  ISR: chEvtSignalI(EVT_HTIF / EVT_TCIF)    │
 *        │  Fill: buf[i]=sample, buf[i+1]=0 (R=0)     │
 *        │  Sources: synth (tone/sequence) | mp3      │
 *        └──────────────────┬─────────────────────────┘
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
#include <etl/queue.h>
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

/** @brief Idle poll interval of the player thread (see the watchdog in player_thread). */
static constexpr uint32_t kIdlePollMs = 250U;

/** @brief Number of playback requests that may wait in the queue. */
static constexpr uint8_t kQueueDepth = 4U;

static THD_WORKING_AREA(s_player_wa, 3072U);
static thread_t* s_player_thd = nullptr;

/**
 * @brief Playback queue (ETL container, statically allocated, no heap).
 *
 * @note  There is deliberately only ONE queue: queued requests wait in FIFO order,
 *        while a request whose definition carries `preempt` (alerts such as
 *        EMERGENCY) stops the running sound, drops everything queued and starts
 *        immediately.
 *
 *        Every access happens under chSysLock(): ETL containers are not thread-safe,
 *        and the player thread must never see a half-updated ring.
 */
static etl::queue<SoundDefinition, kQueueDepth> s_queue;

/* Master volume (0-100, written by set_volume from any thread, read by the player thread) */
static etl::atomic<uint8_t> s_master_volume{100U};

/* Playing flag — written by the player thread, read by is_playing() from any thread */
static etl::atomic<bool> s_playing{false};

/* Runtime sound overrides (SoundId -> definition), written by the SoundService
   during configuration and read by load_sound_definition() from play_sound_id(). */
static MUTEX_DECL(s_override_mutex);
static SoundDefinition s_sound_overrides[SoundId_count];
static bool s_override_valid[SoundId_count];

/* Active playback source — owned by player thread.
   NOTE: this must stay in AXI SRAM (.bss). It contains LittleFS/DMA buffers
   (the MP3 File's cache buffer, the MP3 read-ahead buffer), and the flash driver
   reads/writes them via MDMA. The tightly coupled TCM memories are CPU-only and
   NOT reachable by any DMA: placing it in .dtcm makes the flash MDMA transfer
   error out (STM32_WSPI_MDMA_ERROR_HOOK -> osalSysHalt).

   MEMORY (~36 KB, dominated by dr_mp3's ~23 KB decoder state incl. its 16 KB
   decode scratch): all robot variants share one unified binary, so this is
   reserved on EVERY board — also on boards without a sound amplifier. Together
   with the other sound objects (player WA 4 KB, SoundService incl. WA 5 KB,
   overrides ~2 KB, s_audio_buf 2 KB) soundless boards carry ~50 KB RAM and
   ~28 KB flash that they never use. ~46 KB of that RAM could be reclaimed if it
   ever becomes a problem:
     (1) call player_init() only when the carrier board actually has an
         amplifier — carrier_board_info is already read in InitGlobals(), i.e.
         before main.cpp calls player_init(), and the sound API is null-safe
         while the player is not initialised;
     (2) allocate this object with `new` in player_init() (heap, 54 KB free)
         instead of holding it statically, and create the player thread from
         allocated storage (mind PORT_WORKING_AREA_ALIGN when allocating).
   The flash share can only be removed with per-robot builds
   (-DENABLE_SOUND=0); the firmware currently builds one unified binary. */
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

/**
 * @brief Start playing @p def.
 *
 * The caller hands over a private copy (see next_request): a preempting enqueue()
 * may reset the queue and reuse the slot at any time, so this must not read a slot.
 */
static void play_definition(const SoundDefinition& def) {
  s_source.stop(); /* close any open WAV file */

  if (!s_source.start(def)) {
    ULOG_WARNING("Sound: source start failed (type=%s)", SoundType_to_string(def.type));
    return; /* FILE open/parse failed — remain idle */
  }

  /* Pre-fill both halves before starting BDMA so DMA has valid data immediately. */
  s_source.fill(s_audio_buf, SOUND_HALF_SIZE, s_master_volume.load());
  s_source.fill(s_audio_buf + SOUND_HALF_SIZE, SOUND_HALF_SIZE, s_master_volume.load());

  i2sStartExchange(&I2SD6);
  s_playing.store(true);

  /* The only place that knows a sound actually reached the I2S/DMA stage — the
     host-side RPC log only proves the request was queued. */
  if (def.type == SoundType::TONE) {
    ULOG_INFO("Sound: play tone %hu Hz %hu ms vol %hhu unison %hhu preempt=%u (master %hhu)", def.tone.freq,
              def.tone.duration_ms, def.volume, def.unison, def.preempt ? 1U : 0U, s_master_volume.load());
  } else if (def.type == SoundType::SEQUENCE) {
    /* Only a sequence has an envelope; an MP3 definition's union holds the path. */
    ULOG_INFO(
        "Sound: play sequence vol %hhu %s unison %hhu detune %hu attack %hhu ms decay %hhu ms preempt=%u (master %hhu)",
        def.volume, Waveform_to_string(def.waveform), def.unison, def.detune_hz, def.sequence.attack_ms,
        def.sequence.decay_ms, def.preempt ? 1U : 0U, s_master_volume.load());
  } else {
    ULOG_INFO("Sound: play %s vol %hhu unison %hhu preempt=%u (master %hhu)", SoundType_to_string(def.type), def.volume,
              def.unison, def.preempt ? 1U : 0U, s_master_volume.load());
  }
}

/*===========================================================================*/
/* Internal: dequeue and play the next pending request (if any).            */
/*===========================================================================*/

/**
 * @brief Play the next queued request (no-op while a sound is playing).
 *
 * pop_into() copies the request out and removes it in one step — done under the lock
 * so a preempting enqueue() cannot clear/reuse the slot underneath the copy.
 */
static void dequeue_and_play() {
  if (s_playing.load()) return;

  SoundDefinition def{};
  chSysLock();
  const bool have_request = !s_queue.empty();
  if (have_request) {
    s_queue.pop_into(def);
  }
  chSysUnlock();

  if (have_request) {
    play_definition(def);
  }
}

/*===========================================================================*/
/* Internal: handle a newly arrived EVT_REQUEST.                            */
/*===========================================================================*/

/**
 * @brief Handle a newly arrived EVT_REQUEST.
 *
 * A preempting request takes over even while something is playing; a normal one is
 * left in the queue and picked up when the player becomes idle again.
 */
static void handle_request() {
  bool preempt = false;
  chSysLock();
  if (!s_queue.empty()) {
    preempt = s_queue.front().preempt;
  }
  chSysUnlock();

  if (!preempt) {
    if (s_playing.load()) {
      ULOG_INFO("Sound: request waits behind the running sound");
      return;
    }
    dequeue_and_play();
    return;
  }

  ULOG_INFO("Sound: preempt request (playing=%u)", s_playing.load() ? 1U : 0U);
  if (s_playing.load()) {
    i2sStopExchange(&I2SD6);
    s_playing.store(false);
    s_source.stop();
  }
  dequeue_and_play();
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
    /* The timeout turns the thread into its own watchdog: a request that arrived
       while the player believed it was busy (stale s_playing) or that was missed
       altogether is picked up here, instead of waiting for a DMA event that will
       never come. */
    const eventmask_t ev =
        chEvtWaitAnyTimeout(EVT_HTIF | EVT_TCIF | EVT_REQUEST | EVT_STOP_PLAYBACK, TIME_MS2I(kIdlePollMs));

    if (ev == 0U) {
      dequeue_and_play(); /* no-op when idle and nothing is queued */
      continue;
    }

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

/**
 * @brief Queue a playback request (safe from any thread context).
 *
 * @return QUEUED when the request was accepted, otherwise why it was dropped
 */
static PlayResult enqueue(const SoundDefinition& def) {
  if (s_player_thd == nullptr) return PlayResult::PLAYER_NOT_RUNNING;

  PlayResult result = PlayResult::QUEUED;
  chSysLock();
  if (def.preempt) {
    /* Alerts take over: everything queued is dropped. The running sound itself is
       stopped by the player thread when it handles the request. */
    s_queue.clear();
  } else if (s_queue.full()) {
    result = PlayResult::QUEUE_FULL; /* drop this request instead of delaying the queued ones */
  }
  if (result == PlayResult::QUEUED) {
    /* Not full here: either just cleared or checked above. ETL's push() asserts on a
       full queue, so the check has to come first. */
    s_queue.push(def);
  }
  chEvtSignalI(s_player_thd, EVT_REQUEST);
  /* Required after I-class calls that may have made a higher-priority thread
     ready: chSysUnlock() asserts "priority order violation" if we skip this. */
  chSchRescheduleS();
  chSysUnlock();
  return result;
}

/*===========================================================================*/
/* Public API.                                                               */
/*===========================================================================*/

void player_init() {
  if (s_player_thd != nullptr) return; /* idempotent */

  i2sStart(&I2SD6, &s_i2s_cfg);

  s_player_thd = chThdCreateStatic(s_player_wa, sizeof(s_player_wa), NORMALPRIO + 1, player_thread, nullptr);

  s_source.volume = 80U;
  s_source.synth.set_unison(1U, 0U);

  // Apply persisted overrides (if any) so early sounds already use the
  // last-known high-level definitions instead of the ROM defaults.
  load_sound_overrides_from_storage();

  ULOG_INFO("Sound: player started (sample_rate=%u, volume=%hhu)", SAMPLE_RATE, s_master_volume.load());
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

PlayResult play_sound_id(SoundId id) {
  const uint8_t idx = static_cast<uint8_t>(id);
  if (idx >= SoundId_count) return PlayResult::INVALID_ARGUMENT;

  /* Prefer a flash override; otherwise fall back to the ROM default. */
  SoundDefinition def;
  const bool has_override = load_sound_definition(id, def);
  if (!has_override) {
    def = kDefaultSoundDefs[idx];
  }
  return enqueue(def);
}

PlayResult play_tone(uint16_t freq, uint16_t duration_ms, uint8_t volume, bool preempt) {
  if (freq == 0U || duration_ms == 0U) return PlayResult::INVALID_ARGUMENT;

  SoundDefinition def{};
  def.type = SoundType::TONE;
  def.volume = volume;
  def.preempt = preempt;
  def.tone.freq = freq;
  def.tone.duration_ms = duration_ms;

  return enqueue(def);
}

PlayResult play_file(const char* path, bool preempt) {
  if (path == nullptr) return PlayResult::INVALID_ARGUMENT;

  SoundDefinition def{};
  def.type = SoundType::MP3;
  def.volume = kFileVolume; /* pre-mastered at full scale; master volume scales it */
  def.preempt = preempt;
  strncpy(def.path, path, kMaxPath - 1U);
  def.path[kMaxPath - 1U] = '\0';

  return enqueue(def);
}

PlayResult play_sequence(const Note* notes, uint8_t count, Waveform waveform, uint8_t volume, uint8_t unison,
                         uint16_t detune_hz, uint8_t attack_ms, uint8_t decay_ms, bool preempt) {
  if (notes == nullptr || count == 0U) return PlayResult::INVALID_ARGUMENT;
  if (count > kMaxNotes) count = kMaxNotes;

  SoundDefinition def{};
  def.type = SoundType::SEQUENCE;
  def.waveform = waveform;
  def.volume = volume;
  def.unison = unison;
  def.detune_hz = detune_hz;
  def.preempt = preempt;
  def.sequence.count = count;
  def.sequence.attack_ms = attack_ms;
  def.sequence.decay_ms = decay_ms;
  for (uint8_t i = 0U; i < count; ++i) {
    def.sequence.notes[i] = notes[i];
  }

  return enqueue(def);
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

  ULOG_INFO("Sound: loaded %hhu override(s) from flash (mask=0x%04hx)", loaded, h.valid_mask);
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
    ULOG_INFO("Sound: persisted %hhu override(s) mask=0x%04hx (%d bytes) to %s", count, valid_mask, written,
              kSoundDefsPath);
  }
}

void stop() {
  if (s_player_thd == nullptr) return;
  chSysLock();
  /* Flush pending requests so playback cannot resume after stop. */
  s_queue.clear();
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
