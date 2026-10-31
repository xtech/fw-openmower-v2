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
#include <hal_trng.h>
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

/* Persisted sound-definition overrides (LittleFS) — survive a reboot.
 *
 * Format: a header, then one record per overridden sound — a SoundId byte followed by
 * the raw SoundDefinition bytes (the layout is pinned by the static_assert in
 * sound_definition.hpp).  Only the sounds the high level actually pushed are stored, so
 * the file stays small (16 B + n × 81 B instead of a full SoundId_count table) and no
 * validity bitmask is needed: the record's id says which sound it replaces.
 *
 * header_size and record_size are part of the header on purpose: any future layout change
 * is then *detected* and the file is ignored with a warning, instead of being read with
 * the wrong offsets and silently producing garbage definitions. */
static constexpr const char* kSoundDefsPath = "/cfg/sound_defs.bin";
static constexpr const char* kSoundDefsTmpPath = "/cfg/sound_defs.bin.tmp";
static constexpr uint32_t kSoundDefsMagic = 0x53444631U;  // "SDF1"
static constexpr uint16_t kSoundDefsVersion = 2U;

/** Bytes of one stored override: its SoundId plus the raw definition. */
static constexpr size_t kOverrideRecordSize = sizeof(uint8_t) + sizeof(SoundDefinition);

/** On-disk layout of the persisted sound overrides. */
struct PersistedDefsHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t count;        ///< Number of override records that follow
  uint16_t header_size;  ///< sizeof(PersistedDefsHeader) — guards a header change
  uint16_t record_size;  ///< kOverrideRecordSize — guards a definition change
};
static_assert(sizeof(PersistedDefsHeader) == 12U, "PersistedDefsHeader layout changed — the store format is affected");

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

/*===========================================================================*/
/* Repeating sounds (SoundDefinition::repeat_ms > 0).                        */
/*===========================================================================*/

/**
 * @note  The player owns the repetition: it keeps the definition and restarts the
 *        source itself.  Nothing goes through s_queue, so a repeating sound cannot
 *        fill the queue — it only ever occupies the single request that started it.
 *        The queue always wins: as soon as a request is waiting (or a new sound
 *        starts, or stop() is called) the repetition ends.
 */
static SoundDefinition s_repeat_def{};  ///< Definition being repeated
static systime_t s_repeat_start = 0U;   ///< When the running iteration started
static bool s_repeat_active = false;    ///< true while s_repeat_def owns the player

/** @brief Ticks until the next repeat iteration; 0 when it is due. */
static systime_t repeat_ticks_left() {
  const systime_t period = TIME_MS2I(s_repeat_def.repeat_ms);
  const systime_t elapsed = chVTTimeElapsedSinceX(s_repeat_start);
  return (elapsed >= period) ? static_cast<systime_t>(0U) : static_cast<systime_t>(period - elapsed);
}

/** @brief true while a request is waiting in the queue. */
static bool queue_has_request() {
  chSysLock();
  const bool queued = !s_queue.empty();
  chSysUnlock();
  return queued;
}

/*===========================================================================*/
/* Pause after a sound (SoundDefinition::pause_after_ms > 0).                 */
/*===========================================================================*/

/**
 * @note  The pause belongs to the sound that just ended: on_source_end() starts it,
 *        the idle tick (pump_player) lets the next request through once it is over.
 *        A preempting request cuts it short like it cuts the sound itself, stop()
 *        discards it, and a repeating sound never pauses between its own iterations
 *        (repeat_ms and pause_after_ms are independent).
 */
static uint16_t s_pause_ms = 0U;      ///< pause_after_ms of the sound that started last
static bool s_pause_active = false;   ///< true while the player owes that pause
static systime_t s_pause_start = 0U;  ///< when the sound ended

/** @brief Ticks left of the pause after the last sound; 0 when it is over or not owed. */
static systime_t pause_ticks_left() {
  if (!s_pause_active) return static_cast<systime_t>(0U);
  const systime_t period = TIME_MS2I(s_pause_ms);
  const systime_t elapsed = chVTTimeElapsedSinceX(s_pause_start);
  return (elapsed >= period) ? static_cast<systime_t>(0U) : static_cast<systime_t>(period - elapsed);
}

/** @brief Drop a pending pause (a preempting request, stop(), or the pause being over). */
static void clear_pause() {
  s_pause_active = false;
  s_pause_ms = 0U;
}

/* Runtime sound overrides (SoundId -> definition), written by the SoundService
   during configuration and read by load_sound_definition() from play_sound_id(). */
static MUTEX_DECL(s_override_mutex);
static SoundDefinition s_sound_overrides[SoundId_count];
static bool s_override_valid[SoundId_count];

/* Take lists (persisted, see sound_player.hpp): the index stays in RAM (4 B per
   sound), the names are read from the store when the sound plays. The same mutex
   guards the index and the overrides. */
/* Store location and identity (used by the reader below and by the writer). */
static constexpr const char* kVariantsPath = "/cfg/sound_variants.bin";
static constexpr const char* kVariantsTmpPath = "/cfg/sound_variants.bin.tmp";
static constexpr uint32_t kVariantsMagic = 0x53445631U;  // "SDV1"
static constexpr uint16_t kVariantsVersion = 1U;

struct PersistedVariantRef {
  uint16_t offset;  ///< byte offset of take 1 in the store (0 = no takes)
  uint8_t count;    ///< number of takes
  uint8_t pick;     ///< PickMode
};
static_assert(sizeof(PersistedVariantRef) == 4U, "PersistedVariantRef layout changed - the store format is affected");

struct PersistedVariantsHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t index_size;  ///< sizeof(PersistedVariantRef) - guards an index change
  uint16_t name_size;   ///< kVariantNameMax - guards a name-length change
  uint16_t reserved;
};
static_assert(sizeof(PersistedVariantsHeader) == 12U,
              "PersistedVariantsHeader layout changed - the store format is affected");

/** Bytes of the store before the names start (header + one ref per SoundId). */
static constexpr uint16_t kVariantsDataOffset =
    static_cast<uint16_t>(sizeof(PersistedVariantsHeader) + (SoundId_count * sizeof(PersistedVariantRef)));

static PersistedVariantRef s_variant_ref[SoundId_count];  ///< index of the persisted store
static uint8_t s_variant_last[SoundId_count];             ///< ROTATE: the take that played last

/* Write state of a staged store (begin_sound_variants_write .. commit/abort). */
static File s_variant_out;              ///< the staged store (writer side)
static File s_variant_in;               ///< the store readers share this one
static MUTEX_DECL(s_variant_io_mutex);  ///< serialises the readers
static bool s_variant_writing = false;
static uint16_t s_variant_next_offset = kVariantsDataOffset;
static PersistedVariantRef s_variant_pending[SoundId_count];
static bool s_variant_set_open = false;
static uint8_t s_variant_set_id = 0U;
static uint16_t s_variant_set_offset = 0U;
static uint8_t s_variant_set_count = 0U;

/*===========================================================================*/
/* Take selection: a tiny PRNG.                                              */
/*===========================================================================*/

/*
 * rand() from newlib is a linear congruential generator, and its seed is a boot
 * tick that is nearly the same on every boot. A xorshift32 mixes the bits a "% count"
 * uses and needs no library call.
 *
 * The state is seeded once at boot from the MCU's true random generator and advances
 * on every play, so consecutive plays differ.
 */
static uint32_t s_random_state = 0x2545F491U; /* xorshift32 must never be 0 */

static uint32_t random_next() {
  uint32_t x = s_random_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  s_random_state = x;
  return x;
}

static void random_seed() {
  uint32_t word = 0U;
  if (trngStart(&TRNGD1, nullptr) == HAL_RET_SUCCESS) {
    /* ChibiOS' TRNG LLD reports a timeout as true and success as false. */
    const bool timed_out = trngGenerate(&TRNGD1, sizeof(word), reinterpret_cast<uint8_t*>(&word));
    trngStop(&TRNGD1); /* one word at boot is all the take selection needs */
    if (!timed_out && word != 0U) {
      s_random_state = word;
      return;
    }
  }

  /* No word from the TRNG: keep the default state. The selection still varies per
     play, it just repeats the same sequence after the next boot. */
  ULOG_WARNING("Sound: no TRNG word at boot, the take selection sequence repeats itself");
}

/** True for a take name that ends in ".mp3" (case-insensitive). */
static bool has_mp3_suffix(etl::string_view name) {
  if (name.size() <= 4U) return false;
  const etl::string_view ext = name.substr(name.size() - 4U);
  return ext[0] == '.' && (ext[1] == 'm' || ext[1] == 'M') && (ext[2] == 'p' || ext[2] == 'P') && ext[3] == '3';
}

/** Reads the names of one take set. Returns false when the store cannot deliver. */
static bool read_variant_names(const PersistedVariantRef& ref, char names[kMaxVariants][kVariantNameMax]) {
  chMtxLock(&s_variant_io_mutex);
  s_variant_in.close(); /* no-op unless an earlier attempt left it open */
  bool ok = s_variant_in.open(kVariantsPath, LFS_O_RDONLY) == LFS_ERR_OK;
  if (ok) ok = s_variant_in.seek(static_cast<lfs_soff_t>(ref.offset), LFS_SEEK_SET) >= 0;
  for (uint8_t i = 0U; ok && i < ref.count; ++i) {
    ok = s_variant_in.read(names[i], kVariantNameMax) == static_cast<int>(kVariantNameMax);
    names[i][kVariantNameMax - 1U] = '\0';
    if (ok) {
      /* A corrupt store - or a read that raced a commit between its rename and the
         index swap - must not end in playing a garbage path: fall back to take 1. */
      ok = has_mp3_suffix(etl::string_view(names[i]));
    }
  }
  s_variant_in.close();
  chMtxUnlock(&s_variant_io_mutex);
  return ok;
}

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
    s_repeat_active = false; /* never retry a source that cannot start */
    return;                  /* FILE open/parse failed — remain idle */
  }

  // The definition that starts a sound owns the repetition: every new sound replaces
  // the previous one, and a definition without repeat_ms ends it.
  s_repeat_def = def;
  s_repeat_active = (def.repeat_ms > 0U);
  s_repeat_start = chVTGetSystemTimeX();
  // The pause is owed by this sound when it ends (on_source_end).
  s_pause_ms = def.pause_after_ms;

  /* Pre-fill both halves before starting BDMA so DMA has valid data immediately. */
  s_source.fill(s_audio_buf, SOUND_HALF_SIZE, s_master_volume.load());
  s_source.fill(s_audio_buf + SOUND_HALF_SIZE, SOUND_HALF_SIZE, s_master_volume.load());

  i2sStartExchange(&I2SD6);
  s_playing.store(true);

  if (is_repeat) return;

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
    ULOG_INFO("Sound: play %s '%s' vol %hhu preempt=%u (master %hhu)", SoundType_to_string(def.type), def.path,
              def.volume, def.preempt ? 1U : 0U, s_master_volume.load());
  }

  if (def.repeat_ms > 0U) {
    ULOG_INFO("Sound: repeats every %hu ms until another sound plays", def.repeat_ms);
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

  /* A sound that ended with pause_after_ms keeps the next request back: the idle tick
     (pump_player) comes back here once the pause is over, and a preempting request
     cleared it before calling us. */
  if (s_pause_active) {
    if (pause_ticks_left() > 0U) return;
    clear_pause();
  }

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
/* Internal: the running source is exhausted.                                */
/*===========================================================================*/

/**
 * @brief Repeat the sound, or play the next request.
 *
 * The queue wins over the repetition: a waiting request ends it, so a repeating
 * boot ping can never delay a real sound.
 */
static void on_source_end() {
  i2sStopExchange(&I2SD6);
  s_playing.store(false);

  if (!s_repeat_active) {
    /* pause_after_ms: stay silent for that time before the next request starts. Logged
       once per sound — a request that arrives during the pause is delayed just like one
       that was already queued. */
    if (s_pause_ms > 0U) {
      s_pause_active = true;
      s_pause_start = chVTGetSystemTimeX();
      ULOG_INFO("Sound: pausing %hu ms after playback", s_pause_ms);
      return;
    }
    dequeue_and_play();
    return;
  }
  if (queue_has_request()) {
    s_repeat_active = false;
    dequeue_and_play();
    return;
  }
  if (repeat_ticks_left() == 0U) {
    /* Period already over (sound longer than repeat_ms) — start the next iteration
       right away instead of waiting for the idle poll. */
    play_definition(s_repeat_def, true);
    return;
  }
  /* Otherwise the player thread picks the next iteration up in pump_player(). */
}

/**
 * @brief Player is idle: play a queued request, or the next repeat iteration when due.
 *
 * Called from the player thread's idle tick (see player_thread) — a no-op when
 * nothing is queued and no repeat is pending.  A source that just ended goes
 * through on_source_end() instead, which restarts it directly when the period is
 * already over.
 */
static void pump_player() {
  if (!s_repeat_active) {
    dequeue_and_play();
    return;
  }

  /* A waiting request still wins over the repetition. */
  if (repeat_ticks_left() > 0U) {
    dequeue_and_play();
    return;
  }

  s_repeat_active = false; /* ends here; play_definition() re-arms it if it repeats */
  if (queue_has_request()) {
    dequeue_and_play();
    return;
  }
  play_definition(s_repeat_def, true);
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
  // An alert also cuts a pause_after_ms short, like it cuts the running sound.
  clear_pause();
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
       never come.  While a sound is between two repeat iterations it also bounds the
       wait, so the next iteration starts on time (repeat_ms resolution). */
    systime_t wait = TIME_MS2I(kIdlePollMs);
    if (s_repeat_active && !s_playing.load()) {
      const systime_t left = repeat_ticks_left();
      if (left < wait) wait = left;
    }
    /* Likewise bound the wait by an owed pause, so the next queued sound starts right
       after pause_after_ms instead of on the next idle tick. */
    if (s_pause_active && !s_playing.load()) {
      const systime_t left = pause_ticks_left();
      if (left < wait) wait = left;
    }
    const eventmask_t ev = chEvtWaitAnyTimeout(EVT_HTIF | EVT_TCIF | EVT_REQUEST | EVT_STOP_PLAYBACK, wait);

    if (ev == 0U) {
      pump_player(); /* no-op when idle and nothing is queued */
      continue;
    }

    if (ev & EVT_STOP_PLAYBACK) {
      /* Stop current playback; the queues have already been flushed by stop(). */
      s_repeat_active = false; /* an explicit stop also ends a repetition */
      clear_pause();           /* ... and a pending pause_after_ms */
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

  // Seed the take selection; see random_next() for why this is not rand().
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

  /* Several takes for the same event: ask the store which one to play. Only the
     chosen name is copied into the definition, so the working set stays at take 1
     (which is also the fallback when the store cannot deliver). One take or
     "first" needs no flash read at all. */
  if (def.type == SoundType::MP3) {
    PersistedVariantRef ref{};
    chMtxLock(&s_override_mutex);
    ref = s_variant_ref[idx];
    chMtxUnlock(&s_override_mutex);

    const PickMode pick = static_cast<PickMode>(ref.pick);
    if (ref.count > 1U && ref.count <= kMaxVariants && pick != PickMode::FIRST) {
      /* Largest possible list: kMaxVariants x kVariantNameMax = 448 B on the
         caller's stack, transient. */
      char names[kMaxVariants][kVariantNameMax];
      if (read_variant_names(ref, names)) {
        /* Take and PRNG state under the lock: several threads trigger sounds. */
        uint8_t take = 0U;
        chMtxLock(&s_override_mutex);
        if (pick == PickMode::ROTATE) {
          take = static_cast<uint8_t>((s_variant_last[idx] + 1U) % ref.count);
        } else {
          take = static_cast<uint8_t>(random_next() % ref.count);
        }
        s_variant_last[idx] = take;
        chMtxUnlock(&s_override_mutex);
        strcpy(def.path, "/sounds/");
        strncat(def.path, names[take], kMaxPath - sizeof("/sounds/"));
      } else {
        ULOG_WARNING("Sound: take store unreadable, '%s' plays its first take", SoundId_to_string(id));
      }
    }
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
  def.preempt = preempt;
  def.repeat_ms = repeat_ms;
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
  static File file;
  SoundDefinition def{};

  file.close();  // no-op unless an earlier attempt left it open
  if (file.open(kSoundDefsPath, LFS_O_RDONLY) != LFS_ERR_OK) {
    ULOG_INFO("Sound: no persisted overrides ('%s' missing), using ROM defaults", kSoundDefsPath);
    return;  // No persisted overrides — fall back to ROM defaults.
  }

  PersistedDefsHeader h{};
  int n = file.read(&h, sizeof(h));
  if (n != static_cast<int>(sizeof(h)) || h.magic != kSoundDefsMagic || h.version != kSoundDefsVersion ||
      h.header_size != sizeof(PersistedDefsHeader) || h.record_size != kOverrideRecordSize || h.count > SoundId_count) {
    ULOG_WARNING("Sound: defs store invalid, ignoring");
    file.close();
    return;
  }

  // The store lists exactly the overridden sounds, so start from "none"
  clear_sound_overrides();

  uint16_t loaded = 0U;
  for (uint16_t i = 0U; i < h.count; ++i) {
    uint8_t id = 0U;
    const bool record_read = (file.read(&id, sizeof(id)) == static_cast<int>(sizeof(id))) &&
                             (file.read(&def, sizeof(def)) == static_cast<int>(sizeof(def)));
    if (!record_read || (id >= SoundId_count)) {
      ULOG_WARNING("Sound: defs store truncated, ignoring");
      clear_sound_overrides();  // do not keep a half applied store
      file.close();
      return;
    }
    set_sound_override(static_cast<SoundId>(id), def);
    ++loaded;
  }
  file.close();

  ULOG_INFO("Sound: loaded %hu override(s) from flash", loaded);
}

// A flash failure, as opposed to a request the filesystem rejected (e.g. a full one)
static bool is_flash_error(int err) {
  return err == LFS_ERR_IO || err == LFS_ERR_CORRUPT;
}

// Writes one field of the staged store and remembers its first failing write.
static bool write_staged(File& file, int& write_error, const void* data, size_t size) {
  const int written = file.write(const_cast<void*>(data), size);  // File::write() takes a non-const pointer
  if (written != static_cast<int>(size) && write_error == LFS_ERR_OK) {
    write_error = (written < 0) ? written : LFS_ERR_IO;
  }
  return written == static_cast<int>(size);
}

void save_sound_overrides_to_storage() {
  // Count first. The header carries it and every valid sound is one record.
  uint16_t count = 0U;
  chMtxLock(&s_override_mutex);
  for (uint8_t i = 0U; i < SoundId_count; ++i) {
    if (s_override_valid[i]) ++count;
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

  // Stage the store in a temp file and rename it over kSoundDefsPath only once it is complete
  if (file.open(kSoundDefsTmpPath, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC) != LFS_ERR_OK) {
    ULOG_WARNING("Sound: cannot open defs store '%s'", kSoundDefsTmpPath);
    return;
  }

  PersistedDefsHeader h{};
  h.magic = kSoundDefsMagic;
  h.version = kSoundDefsVersion;
  h.count = count;
  h.header_size = sizeof(PersistedDefsHeader);
  h.record_size = kOverrideRecordSize;

  // The first failing write decides whether the staged file may be dropped again below.
  int write_error = LFS_ERR_OK;
  bool ok = write_staged(file, write_error, &h, sizeof(h));
  for (uint8_t i = 0U; ok && i < SoundId_count; ++i) {
    SoundDefinition def{};
    chMtxLock(&s_override_mutex);
    const bool valid = s_override_valid[i];
    if (valid) {
      def = s_sound_overrides[i];
    }
    chMtxUnlock(&s_override_mutex);
    if (!valid) continue;

    const uint8_t id = i;
    ok = write_staged(file, write_error, &id, sizeof(id)) && write_staged(file, write_error, &def, sizeof(def));
  }

  // sync() flushes the staged file and commits its size
  const int sync_result = file.sync();
  file.close();

  // A flash failure leaves LittleFS's program cache dirty (lfs_bd_flush() returns without
  // clearing it), and programming another block through that cache hits an LFS_ASSERT — so
  // only touch the filesystem again while the flash is known to be healthy. sync() can still
  // report success in that state, which is why both errors are checked. A staged file that
  // is left behind is harmless: nothing reads it and the next save truncates it again.
  const bool flash_healthy = !is_flash_error(write_error) && !is_flash_error(sync_result);

  if (!ok || sync_result != LFS_ERR_OK) {
    ULOG_WARNING("Sound: defs store write failed (write=%d, sync=%d), previous store kept", write_error, sync_result);
    if (flash_healthy) lfs_remove(&lfs, kSoundDefsTmpPath);  // best effort: drop the partial file
    return;
  }

  const int rename_result = lfs_rename(&lfs, kSoundDefsTmpPath, kSoundDefsPath);
  if (rename_result != LFS_ERR_OK) {
    ULOG_WARNING("Sound: cannot replace defs store (%d), previous store kept", rename_result);
    if (flash_healthy && !is_flash_error(rename_result)) lfs_remove(&lfs, kSoundDefsTmpPath);  // best effort
    return;
  }

  const unsigned int file_bytes = static_cast<unsigned int>(sizeof(h) + (count * kOverrideRecordSize));
  ULOG_INFO("Sound: persisted %hu override(s) (%u bytes) to %s", count, file_bytes, kSoundDefsPath);
}

/*===========================================================================*/
/* Take lists: the LittleFS store.                                           */
/*===========================================================================*/

/*
 * Layout of /cfg/sound_variants.bin:
 *
 *   header (12 B: magic, version, index_size, name_size)
 *   index  (SoundId_count x PersistedVariantRef, fixed position per sound)
 *   data   (per take one NUL padded kVariantNameMax name; the "/sounds/" prefix
 *           is a low level contract and is not stored)
 *
 * The index sits at a fixed position, so an entry is one seek away and no search
 * is needed. Written like the definition store: staged in a .tmp file that is
 * renamed on commit, so a rejected blob or a power loss leaves the previous lists
 * in place. A header whose sizes do not match the compiled layout is ignored,
 * which simply means every sound plays its first take.
 */

static PersistedVariantsHeader variant_header() {
  PersistedVariantsHeader h{};
  h.magic = kVariantsMagic;
  h.version = kVariantsVersion;
  h.index_size = sizeof(PersistedVariantRef);
  h.name_size = static_cast<uint16_t>(kVariantNameMax);
  return h;
}

void begin_sound_variants_write() {
  if (s_variant_writing) abort_sound_variants_write();
  for (uint8_t i = 0U; i < SoundId_count; ++i) {
    s_variant_pending[i] = PersistedVariantRef{};
  }
  s_variant_set_open = false;
  s_variant_set_count = 0U;
  s_variant_next_offset = kVariantsDataOffset;

  /* mkdirp() lives on File; borrow the reader instance for that one call. */
  chMtxLock(&s_variant_io_mutex);
  const int mkdir_result = s_variant_in.mkdirp(kVariantsPath);
  chMtxUnlock(&s_variant_io_mutex);
  if (mkdir_result != LFS_ERR_OK) {
    ULOG_WARNING("Sound: cannot create the take store dir");
    return;
  }
  if (s_variant_out.open(kVariantsTmpPath, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC) != LFS_ERR_OK) {
    ULOG_WARNING("Sound: cannot open the take store '%s'", kVariantsTmpPath);
    return;
  }

  /* Reserve header and index; the commit writes both again. */
  int err = LFS_ERR_OK;
  const PersistedVariantsHeader h = variant_header();
  bool ok = write_staged(s_variant_out, err, &h, sizeof(h));
  for (uint8_t i = 0U; ok && i < SoundId_count; ++i) {
    const PersistedVariantRef zero{};
    ok = write_staged(s_variant_out, err, &zero, sizeof(zero));
  }
  if (!ok) {
    ULOG_WARNING("Sound: take store write failed (%d)", err);
    s_variant_out.close();
    lfs_remove(&lfs, kVariantsTmpPath); /* best effort */
    return;
  }
  s_variant_writing = true;
}

void begin_sound_variant_set(SoundId id) {
  const uint8_t idx = static_cast<uint8_t>(id);
  if (idx >= SoundId_count) return;
  s_variant_set_open = true;
  s_variant_set_id = idx;
  s_variant_set_offset = s_variant_next_offset;
  s_variant_set_count = 0U;
}

bool add_sound_variant(etl::string_view name) {
  /* Without a staged store (read-only flash, ...) the definitions still apply; the
     takes are simply not kept. A take outside an open set, though, is a broken
     stream and has to reject the blob. */
  if (!s_variant_writing) return true;
  if (!s_variant_set_open) {
    ULOG_ERROR("Sound config: take outside a 'variants' array");
    return false;
  }
  if (name.size() + 1U > kVariantNameMax) {
    ULOG_ERROR("Sound config: take name too long (%.*s)", static_cast<int>(name.size()), name.data());
    return false;
  }
  /* The player concatenates the name with "/sounds/", so an empty name or one
     that is not an MP3 has to fail here instead of at play time. */
  if (!has_mp3_suffix(name)) {
    ULOG_ERROR("Sound config: take '%.*s' is not an .mp3 file name", static_cast<int>(name.size()), name.data());
    return false;
  }
  if (s_variant_set_count >= kMaxVariants) {
    ULOG_ERROR("Sound config: more than %hhu takes for one sound", kMaxVariants);
    return false;
  }

  /* Fixed stride, NUL padded - the reader indexes it without a length table. */
  char record[kVariantNameMax]{};
  for (size_t i = 0U; i < name.size(); ++i) {
    record[i] = name[i];
  }

  int err = LFS_ERR_OK;
  if (!write_staged(s_variant_out, err, record, sizeof(record))) {
    ULOG_WARNING("Sound: take store write failed (%d)", err);
    return false;
  }
  s_variant_set_count++;
  s_variant_next_offset = static_cast<uint16_t>(s_variant_next_offset + sizeof(record));
  return true;
}

void end_sound_variant_set() {
  if (s_variant_set_open && s_variant_set_count > 0U) {
    s_variant_pending[s_variant_set_id].offset = s_variant_set_offset;
    s_variant_pending[s_variant_set_id].count = s_variant_set_count;
  }
  s_variant_set_open = false;
  s_variant_set_count = 0U;
}

void set_sound_variant_pick(SoundId id, PickMode pick) {
  const uint8_t idx = static_cast<uint8_t>(id);
  if (idx >= SoundId_count) return;
  s_variant_pending[idx].pick = static_cast<uint8_t>(pick);
}

void commit_sound_variants_write() {
  if (!s_variant_writing) return;
  s_variant_writing = false;

  int err = LFS_ERR_OK;
  bool ok = s_variant_out.seek(0, LFS_SEEK_SET) >= 0;
  const PersistedVariantsHeader h = variant_header();
  ok = ok && write_staged(s_variant_out, err, &h, sizeof(h));
  for (uint8_t i = 0U; ok && i < SoundId_count; ++i) {
    ok = write_staged(s_variant_out, err, &s_variant_pending[i], sizeof(PersistedVariantRef));
  }
  const int sync_result = s_variant_out.sync();
  s_variant_out.close();

  const bool flash_healthy = !is_flash_error(err) && !is_flash_error(sync_result);
  if (!ok || sync_result != LFS_ERR_OK) {
    ULOG_WARNING("Sound: take store write failed (write=%d, sync=%d), previous store kept", err, sync_result);
    if (flash_healthy) lfs_remove(&lfs, kVariantsTmpPath); /* best effort */
    return;
  }
  const int rename_result = lfs_rename(&lfs, kVariantsTmpPath, kVariantsPath);
  if (rename_result != LFS_ERR_OK) {
    ULOG_WARNING("Sound: cannot replace the take store (%d), previous store kept", rename_result);
    if (flash_healthy && !is_flash_error(rename_result)) lfs_remove(&lfs, kVariantsTmpPath);
    return;
  }
  load_sound_variants_from_storage();
}

void abort_sound_variants_write() {
  if (!s_variant_writing) return;
  s_variant_writing = false;
  s_variant_set_open = false;
  s_variant_out.close();
  lfs_remove(&lfs, kVariantsTmpPath); /* best effort */
}

void load_sound_variants_from_storage() {
  PersistedVariantRef refs[SoundId_count]{};
  PersistedVariantsHeader h{};

  chMtxLock(&s_variant_io_mutex);
  s_variant_in.close(); /* no-op unless an earlier attempt left it open */
  bool ok = s_variant_in.open(kVariantsPath, LFS_O_RDONLY) == LFS_ERR_OK;
  if (ok) ok = s_variant_in.read(&h, sizeof(h)) == static_cast<int>(sizeof(h));
  if (ok) {
    ok = h.magic == kVariantsMagic && h.version == kVariantsVersion && h.index_size == sizeof(PersistedVariantRef) &&
         h.name_size == kVariantNameMax;
  }
  if (ok) ok = s_variant_in.read(refs, sizeof(refs)) == static_cast<int>(sizeof(refs));
  const int file_size = ok ? s_variant_in.size() : 0;
  s_variant_in.close();
  chMtxUnlock(&s_variant_io_mutex);

  chMtxLock(&s_override_mutex);
  uint8_t with_takes = 0U;
  for (uint8_t i = 0U; i < SoundId_count; ++i) {
    PersistedVariantRef r = ok ? refs[i] : PersistedVariantRef{};
    /* A truncated or foreign file must not make the player read past its end. */
    const uint32_t end = static_cast<uint32_t>(r.offset) + (static_cast<uint32_t>(r.count) * kVariantNameMax);
    if (r.count == 0U || r.count > kMaxVariants || end > static_cast<uint32_t>(file_size)) {
      r = PersistedVariantRef{};
    }
    if (r.count > 0U) with_takes++;
    s_variant_ref[i] = r;
  }
  chMtxUnlock(&s_override_mutex);

  if (ok) {
    ULOG_INFO("Sound: %hhu take list(s) restored from %s", with_takes, kVariantsPath);
  } else {
    ULOG_INFO("Sound: no usable take store ('%s'), every sound plays its first take", kVariantsPath);
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
