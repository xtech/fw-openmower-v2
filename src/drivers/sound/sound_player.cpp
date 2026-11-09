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
 *        │  PlaybackSequencer (sound_playback.hpp):   │
 *        │  FIFO queue, preempt, repeat, pause_after  │
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
#include <ulog.h>

#include <cstring>

#include "hal.h"
#include "sound_definition.hpp"
#include "sound_playback.hpp"
#include "sound_random.hpp"
#include "sound_source.hpp"
#include "sound_storage.hpp"
#include "sound_variants.hpp"

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

/** @brief Idle poll interval of the player thread (see the watchdog in player_thread). */
static constexpr uint32_t kIdlePollMs = 250U;

static THD_WORKING_AREA(s_player_wa, 3072U);
static thread_t* s_player_thd = nullptr;

/** @brief The playback policy (queue, preempt, repeat, pause), guarded by s_playback_mutex. */
static PlaybackSequencer s_playback;
static MUTEX_DECL(s_playback_mutex);

/**
 * @brief Monotonic milliseconds for the playback deadlines.
 *
 * The 32-bit system tick counter wraps. Converting only the elapsed tick difference and
 * keeping the unconverted sub-millisecond remainder makes the millisecond clock
 * continuous across that wrap; the millisecond value itself wraps naturally.
 */
static uint32_t now_ms() {
  static systime_t s_last_ticks = 0;
  static uint32_t s_sub_ms_remainder = 0;  ///< remainder in 1/1000 tick units
  static uint32_t s_ms = 0;
  static bool s_initialized = false;

  const systime_t ticks = chVTGetSystemTimeX();
  if (!s_initialized) {
    s_initialized = true;
    s_last_ticks = ticks;
    return 0;
  }

  const uint32_t delta = static_cast<uint32_t>(ticks - s_last_ticks);
  s_last_ticks = ticks;
  const uint64_t total = static_cast<uint64_t>(s_sub_ms_remainder) + (static_cast<uint64_t>(delta) * 1000U);
  s_ms += static_cast<uint32_t>(total / CH_CFG_ST_FREQUENCY);
  s_sub_ms_remainder = static_cast<uint32_t>(total % CH_CFG_ST_FREQUENCY);
  return s_ms;
}

/* Master volume (0-100, written by set_volume from any thread, read by the player thread) */
static etl::atomic<uint8_t> s_master_volume{100U};

/* Playing flag — written by the player thread, read by is_playing() from any thread */
static etl::atomic<bool> s_playing{false};

/*===========================================================================*/
/* Playback policy                                                           */
/*===========================================================================*/

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
 *
 * @param is_repeat true for a repeat iteration of the same definition (kept quiet, a
 *                  repeating 2 s ping would flood the log otherwise).
 */
static void play_definition(const SoundDefinition& def, bool is_repeat = false) {
  s_source.stop(); /* close any open WAV file */

  if (!s_source.start(def)) {
    ULOG_WARNING("Sound: source start failed (type=%s)", SoundType_to_string(def.type));
    /* Never retry a source that cannot start: drop the repetition the sequencer armed. */
    chMtxLock(&s_playback_mutex);
    s_playback.CancelRepeat();
    chMtxUnlock(&s_playback_mutex);
    return; /* FILE open/parse failed — remain idle */
  }

  /* Pre-fill both halves before starting BDMA so DMA has valid data immediately. The
     sequencer already armed repeat_ms/pause_after_ms for this definition. */
  s_source.fill(s_audio_buf, SOUND_HALF_SIZE, s_master_volume.load());
  s_source.fill(s_audio_buf + SOUND_HALF_SIZE, SOUND_HALF_SIZE, s_master_volume.load());

  i2sStartExchange(&I2SD6);
  s_playing.store(true);

  if (is_repeat) return;

  /* The only place that knows a sound actually reached the I2S/DMA stage — the
     host-side RPC log only proves the request was queued. */
  if (def.type == SoundType::TONE) {
    ULOG_INFO("Sound: play tone %hu Hz %hu ms vol %hhu unison %hhu (master %hhu)", def.tone.freq, def.tone.duration_ms,
              def.volume, def.unison, s_master_volume.load());
  } else if (def.type == SoundType::SEQUENCE) {
    /* Only a sequence has an envelope; an MP3 definition's union holds the path. */
    ULOG_INFO("Sound: play sequence vol %hhu %s unison %hhu detune %hu attack %hhu ms decay %hhu ms (master %hhu)",
              def.volume, Waveform_to_string(def.waveform), def.unison, def.detune_hz, def.sequence.attack_ms,
              def.sequence.decay_ms, s_master_volume.load());
  } else {
    ULOG_INFO("Sound: play %s '%s' vol %hhu (master %hhu)", SoundType_to_string(def.type), def.path, def.volume,
              s_master_volume.load());
  }

  if (def.repeat_ms > 0U) {
    ULOG_INFO("Sound: repeats every %hu ms until another sound plays", def.repeat_ms);
  }
}

/*===========================================================================*/
/* Internal: the playback policy.                                            */
/*===========================================================================*/

static PlaybackStep take_next(uint32_t now, SoundDefinition& out) {
  chMtxLock(&s_playback_mutex);
  const PlaybackStep step = s_playback.TakeNext(now, out);
  chMtxUnlock(&s_playback_mutex);
  return step;
}

static PlaybackStep source_end_step(uint32_t now, SoundDefinition& out) {
  chMtxLock(&s_playback_mutex);
  const PlaybackStep step = s_playback.OnSourceEnd(now, out);
  chMtxUnlock(&s_playback_mutex);
  return step;
}

static PlaybackStep tick_step(uint32_t now, SoundDefinition& out) {
  chMtxLock(&s_playback_mutex);
  const PlaybackStep step = s_playback.OnTick(now, out);
  chMtxUnlock(&s_playback_mutex);
  return step;
}

static bool front_preempts() {
  chMtxLock(&s_playback_mutex);
  const bool preempt = s_playback.FrontPreempts();
  chMtxUnlock(&s_playback_mutex);
  return preempt;
}

static uint32_t pause_left(uint32_t now) {
  chMtxLock(&s_playback_mutex);
  const uint32_t left = s_playback.TimeUntilPause(now);
  chMtxUnlock(&s_playback_mutex);
  return left;
}

static uint32_t repeat_left(uint32_t now) {
  chMtxLock(&s_playback_mutex);
  const uint32_t left = s_playback.TimeUntilRepeat(now);
  chMtxUnlock(&s_playback_mutex);
  return left;
}

static void run_step(PlaybackStep step, const SoundDefinition& def) {
  if (step == PlaybackStep::START) {
    play_definition(def);
  } else if (step == PlaybackStep::REPLAY) {
    play_definition(def, true);
  }
}

/** @brief Play the next queued request (no-op while a sound is playing or a pause runs). */
static void start_next_if_idle(uint32_t now) {
  if (s_playing.load()) return;
  SoundDefinition def{};
  run_step(take_next(now, def), def);
}

/*===========================================================================*/
/* Internal: the running source is exhausted.                                */
/*===========================================================================*/

/** @brief Repeat the sound, play the next request, or owe a pause. */
static void on_source_end() {
  i2sStopExchange(&I2SD6);
  s_playing.store(false);

  const uint32_t now = now_ms();
  SoundDefinition def{};
  const PlaybackStep step = source_end_step(now, def);
  if (step == PlaybackStep::NONE) {
    const uint32_t pause = pause_left(now);
    if (pause > 0U) {
      ULOG_INFO("Sound: pausing %u ms after playback", pause);
    }
    return;
  }
  run_step(step, def);
}

/**
 * @brief Player is idle: start a due repeat or a queued request.
 *
 * Called from the player thread's idle tick (see player_thread). A source that just
 * ended goes through on_source_end() instead.
 */
static void pump_player() {
  if (s_playing.load()) return;
  const uint32_t now = now_ms();
  SoundDefinition def{};
  run_step(tick_step(now, def), def);
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
  const uint32_t now = now_ms();

  if (!front_preempts()) {
    if (s_playing.load()) {
      ULOG_INFO("Sound: request waits behind the running sound");
      return;
    }
    start_next_if_idle(now);
    return;
  }

  ULOG_INFO("Sound: preempt request (playing=%u)", s_playing.load() ? 1U : 0U);
  /* An alert also cuts a pause_after_ms short, like it cuts the running sound. */
  chMtxLock(&s_playback_mutex);
  s_playback.Preempt();
  chMtxUnlock(&s_playback_mutex);
  if (s_playing.load()) {
    i2sStopExchange(&I2SD6);
    s_playing.store(false);
    s_source.stop();
  }
  start_next_if_idle(now);
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
       never come.  While a sound is between two repeat iterations it also bounds the
       wait, so the next iteration starts on time (repeat_ms resolution). */
    systime_t wait = TIME_MS2I(kIdlePollMs);
    const uint32_t now = now_ms();
    const uint32_t repeat = repeat_left(now);
    if (!s_playing.load() && repeat != UINT32_MAX) {
      const systime_t left = TIME_MS2I(repeat);
      if (left < wait) wait = left;
    }
    /* Likewise bound the wait by an owed pause, so the next queued sound starts right
       after pause_after_ms instead of on the next idle tick. */
    if (!s_playing.load()) {
      const uint32_t pause = pause_left(now);
      if (pause > 0U) {
        const systime_t left = TIME_MS2I(pause);
        if (left < wait) wait = left;
      }
    }
    const eventmask_t ev = chEvtWaitAnyTimeout(EVT_HTIF | EVT_TCIF | EVT_REQUEST | EVT_STOP_PLAYBACK, wait);

    if (ev == 0U) {
      pump_player(); /* no-op when idle and nothing is queued */
      continue;
    }

    if (ev & EVT_STOP_PLAYBACK) {
      /* Stop the current playback; the queue was already flushed by stop(). Requests
         enqueued after that stop belong to the next action and must survive. */
      chMtxLock(&s_playback_mutex);
      s_playback.CancelPlayback();
      chMtxUnlock(&s_playback_mutex);
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
        on_source_end();
      }
    }

    if (ev & EVT_TCIF) {
      /* DMA finished second half → refill second half */
      s_source.fill(s_audio_buf + SOUND_HALF_SIZE, SOUND_HALF_SIZE, s_master_volume.load());
      if (!s_source.is_active() && s_playing.load()) {
        on_source_end();
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
 * @param def      What to play.
 * @param preempt  Stop the running sound, drop the queue and start immediately. Used by the
 *                ad-hoc RPC commands ("play this now"); machine-driven sounds are queued and
 *                the caller stops the player itself if it needs to cut something off.
 * @return QUEUED when the request was accepted, otherwise why it was dropped
 */
static PlayResult enqueue(const SoundDefinition& def, bool preempt = false) {
  if (s_player_thd == nullptr) return PlayResult::PLAYER_NOT_RUNNING;

  chMtxLock(&s_playback_mutex);
  const bool accepted = s_playback.Enqueue(def, preempt);
  chMtxUnlock(&s_playback_mutex);
  if (!accepted) return PlayResult::QUEUE_FULL;

  chEvtSignal(s_player_thd, EVT_REQUEST);
  return PlayResult::QUEUED;
}

/*===========================================================================*/
/* Public API.                                                               */
/*===========================================================================*/

void player_init() {
  if (s_player_thd != nullptr) return; /* idempotent */

  i2sStart(&I2SD6, &s_i2s_cfg);

  s_player_thd = chThdCreateStatic(s_player_wa, sizeof(s_player_wa), NORMALPRIO + 1, player_thread, nullptr);

  // Seed the take selection; see sound_random.hpp for why this is not rand().
  random_seed();

  s_source.volume = 80U;
  s_source.synth.set_unison(1U, 0U);

  // Apply persisted overrides (if any) so early sounds already use the
  // last-known high-level definitions instead of the ROM defaults.
  load_sound_overrides_from_storage();
  // The take lists are read when a sound plays; only their index is loaded here.
  load_sound_variants_from_storage();

  ULOG_INFO("Sound: player started (sample_rate=%u, volume=%hhu)", SAMPLE_RATE, s_master_volume.load());
}

PlayResult play_sound_id(SoundId id) {
  const uint8_t idx = static_cast<uint8_t>(id);
  if (idx >= SoundId_count) return PlayResult::INVALID_ARGUMENT;

  // Prefer the high-level override (RAM or persisted); otherwise the ROM default.
  SoundDefinition def{};
  if (!get_sound_override(id, def)) {
    if (idx >= kDefaultSoundDefsCount) {
      // The high level has not configured this sound (yet): accepted, but silent.
      return PlayResult::NO_DEFINITION;
    }
    def = kDefaultSoundDefs[idx];
  }

  /* Several takes for the same event: ask the store which one to play. Only the chosen
     name is copied into the definition, so the working set stays at take 1 (which is
     also the fallback when the store cannot deliver). */
  if (def.type == SoundType::MP3) {
    (void)choose_variant(id, def.path, sizeof(def.path));
  }

  return enqueue(def);
}

PlayResult play_tone(uint16_t freq, uint16_t duration_ms, uint8_t volume, bool preempt) {
  if (freq == 0U || duration_ms == 0U) return PlayResult::INVALID_ARGUMENT;

  SoundDefinition def{};
  def.type = SoundType::TONE;
  def.volume = volume;
  def.tone.freq = freq;
  def.tone.duration_ms = duration_ms;

  return enqueue(def, preempt);
}

PlayResult play_file(const char* path, bool preempt) {
  if (path == nullptr) return PlayResult::INVALID_ARGUMENT;

  SoundDefinition def{};
  def.type = SoundType::MP3;
  def.volume = kFileVolume; /* pre-mastered at full scale; master volume scales it */
  strncpy(def.path, path, kMaxPath - 1U);
  def.path[kMaxPath - 1U] = '\0';

  return enqueue(def, preempt);
}

PlayResult play_sequence(const Note* notes, uint8_t count, Waveform waveform, uint8_t volume, uint8_t unison,
                         uint16_t detune_hz, uint8_t attack_ms, uint8_t decay_ms, uint16_t repeat_ms, bool preempt) {
  if (notes == nullptr || count == 0U) return PlayResult::INVALID_ARGUMENT;
  if (count > kMaxNotes) count = kMaxNotes;
  /* The voices are summed around a centre voice, so only odd counts are meaningful; the
     config parser enforces the same list, and checking here keeps the RPC path (and any
     later caller) honest instead of silently playing a different sound than requested. */
  if (unison != 1U && unison != 3U && unison != 5U && unison != 7U) return PlayResult::INVALID_ARGUMENT;

  SoundDefinition def{};
  def.type = SoundType::SEQUENCE;
  def.waveform = waveform;
  def.volume = volume;
  def.unison = unison;
  def.detune_hz = detune_hz;
  def.repeat_ms = repeat_ms;
  def.sequence.count = count;
  def.sequence.attack_ms = attack_ms;
  def.sequence.decay_ms = decay_ms;
  for (uint8_t i = 0U; i < count; ++i) {
    def.sequence.notes[i] = notes[i];
  }

  return enqueue(def, preempt);
}

void set_volume(uint8_t volume) {
  if (volume > 100U) volume = 100U;
  s_master_volume.store(volume);
}

void stop() {
  if (s_player_thd == nullptr) return;
  /* Flush pending requests so playback cannot resume after stop. The player thread stops
     the audio itself when it handles EVT_STOP_PLAYBACK. */
  chMtxLock(&s_playback_mutex);
  s_playback.Stop();
  chMtxUnlock(&s_playback_mutex);
  chEvtSignal(s_player_thd, EVT_STOP_PLAYBACK);
}

bool is_playing() {
  return s_playing.load();
}

}  // namespace xbot::driver::sound
