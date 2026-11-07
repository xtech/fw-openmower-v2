/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sound_playback.hpp
 * @brief Hardware-free playback policy: queue, preemption, repetition and pauses.
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-11-07
 *
 * @note  The player owns the audio (I2S, source, volume); this class only decides *what*
 *        to start *when*. It has no ChibiOS, no hardware and no clock of its own - the
 *        caller passes the same monotonic millisecond clock to every call, which is what
 *        makes the policy testable on the host.
 *
 *        The precedence is deliberate and unchanged:
 *          1. a queued request wins over a repetition,
 *          2. a preempting request clears the queue and cuts the current sound,
 *          3. pause_after_ms delays the next request (but never a repeat iteration),
 *          4. repeat_ms restarts the current definition until another request arrives.
 */

#ifndef SOUND_PLAYBACK_HPP
#define SOUND_PLAYBACK_HPP

#include <etl/queue.h>

#include <cstdint>

#include "sound_definition.hpp"

namespace xbot::driver::sound {

/** @brief One queued request: what to play, and whether it may take over the player. */
struct PlaybackRequest {
  SoundDefinition def;
  bool preempt = false;  ///< stop the running sound and drop the queue, then play this
};

/** @brief What the player should do after one policy evaluation. */
enum class PlaybackStep : uint8_t {
  NONE,    ///< Nothing to start
  START,   ///< Start `definition()` (a queued request)
  REPLAY,  ///< Restart `definition()` (the current repeat iteration)
};

/**
 * @brief The playback policy: FIFO queue, preemption, repetition and pause_after_ms.
 *
 * The player thread owns the audio and serialises access (the class is not thread safe).
 * All times are milliseconds from one monotonic clock; the wrap-around is handled with
 * signed differences, so a 32-bit clock is enough.
 */
class PlaybackSequencer {
 public:
  /** @brief Number of requests that may wait in the queue. */
  static constexpr uint8_t kQueueDepth = 4U;

  /**
   * @brief Queue @p def.
   * @return false when the queue is full and @p preempt is false (the request was dropped).
   */
  bool Enqueue(const SoundDefinition& def, bool preempt);

  /** @brief True when at least one request is waiting. */
  bool HasRequest() const;

  /** @brief True when the request at the front wants to take over the player. */
  bool FrontPreempts() const;

  /**
   * @brief Start the next request when nothing is playing.
   *
   * @param now_ms Monotonic time.
   * @param out    Receives the definition to start.
   * @return START with a valid @p out, or NONE when the queue is empty or a pause is owed.
   */
  PlaybackStep TakeNext(uint32_t now_ms, SoundDefinition& out);

  /**
   * @brief The current source ended: repeat, start the next request, or owe a pause.
   *
   * @param now_ms Monotonic time.
   * @param out    Receives the definition to start for START/REPLAY.
   */
  PlaybackStep OnSourceEnd(uint32_t now_ms, SoundDefinition& out);

  /**
   * @brief Periodic tick while idle: start a due repeat or a queued request.
   *
   * @param now_ms Monotonic time.
   * @param out    Receives the definition to start for START/REPLAY.
   */
  PlaybackStep OnTick(uint32_t now_ms, SoundDefinition& out);

  /** @brief Drop the queue, the repetition and any owed pause. */
  void Stop();

  /** @brief End the current repetition without touching the queue or a pause. */
  void CancelRepeat();

  /** @brief A preempting request takes over: drop an owed pause (the repeat is replaced). */
  void Preempt();

  /** @brief ms until the next repeat iteration; UINT32_MAX when no repeat is active. */
  uint32_t TimeUntilRepeat(uint32_t now_ms) const;

  /** @brief ms until an owed pause is over; 0 when no pause is active. */
  uint32_t TimeUntilPause(uint32_t now_ms) const;

 private:
  /** @brief True while a pause is still owed; clears it once it is over. */
  bool PauseOwed(uint32_t now_ms);

  /** @brief True when the current repeat iteration is due. */
  bool RepeatDue(uint32_t now_ms) const;

  /** @brief Pop the next request, arm its repeat/pause and fill @p out. */
  PlaybackStep StartRequest(uint32_t now_ms, SoundDefinition& out);

  /** @brief Re-arm and return the current definition as REPLAY. */
  PlaybackStep StartRepeat(uint32_t now_ms, SoundDefinition& out);

  etl::queue<PlaybackRequest, kQueueDepth> queue_;
  SoundDefinition current_{};  ///< The definition that started last
  uint32_t repeat_start_ms_ = 0U;
  bool repeat_active_ = false;
  uint16_t pause_ms_ = 0U;
  uint32_t pause_start_ms_ = 0U;
  bool pause_active_ = false;
};

}  // namespace xbot::driver::sound

#endif  // SOUND_PLAYBACK_HPP
