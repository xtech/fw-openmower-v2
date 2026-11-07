/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "sound_playback.hpp"

namespace xbot::driver::sound {

namespace {

/** Signed difference of two ms stamps, so the 49-day wraparound compares right. */
inline bool time_reached(uint32_t now_ms, uint32_t due_ms) {
  return static_cast<int32_t>(now_ms - due_ms) >= 0;
}

}  // namespace

bool PlaybackSequencer::Enqueue(const SoundDefinition& def, bool preempt) {
  if (preempt) {
    queue_.clear();
  } else if (queue_.full()) {
    return false; /* drop this request instead of delaying the queued ones */
  }
  queue_.push(PlaybackRequest{def, preempt});
  return true;
}

bool PlaybackSequencer::HasRequest() const {
  return !queue_.empty();
}

bool PlaybackSequencer::FrontPreempts() const {
  return !queue_.empty() && queue_.front().preempt;
}

bool PlaybackSequencer::PauseOwed(uint32_t now_ms) {
  if (!pause_active_) return false;
  if (!time_reached(now_ms, pause_start_ms_ + pause_ms_)) return true;
  pause_active_ = false;
  pause_ms_ = 0U;
  return false;
}

bool PlaybackSequencer::RepeatDue(uint32_t now_ms) const {
  return repeat_active_ && current_.repeat_ms > 0U && time_reached(now_ms, repeat_start_ms_ + current_.repeat_ms);
}

PlaybackStep PlaybackSequencer::StartRequest(uint32_t now_ms, SoundDefinition& out) {
  if (queue_.empty()) return PlaybackStep::NONE;

  PlaybackRequest request{};
  queue_.pop_into(request);
  current_ = request.def;
  repeat_start_ms_ = now_ms;
  repeat_active_ = (current_.repeat_ms > 0U);
  pause_ms_ = current_.pause_after_ms;
  pause_active_ = false;
  out = current_;
  return PlaybackStep::START;
}

PlaybackStep PlaybackSequencer::StartRepeat(uint32_t now_ms, SoundDefinition& out) {
  repeat_start_ms_ = now_ms;
  pause_ms_ = current_.pause_after_ms;
  pause_active_ = false;
  out = current_;
  return PlaybackStep::REPLAY;
}

PlaybackStep PlaybackSequencer::TakeNext(uint32_t now_ms, SoundDefinition& out) {
  if (PauseOwed(now_ms)) return PlaybackStep::NONE;
  return StartRequest(now_ms, out);
}

PlaybackStep PlaybackSequencer::OnSourceEnd(uint32_t now_ms, SoundDefinition& out) {
  if (!repeat_active_) {
    /* pause_after_ms: stay silent for that time before the next request starts. */
    if (pause_ms_ > 0U) {
      pause_active_ = true;
      pause_start_ms_ = now_ms;
      return PlaybackStep::NONE;
    }
    return StartRequest(now_ms, out);
  }

  /* The queue wins over the repetition: a waiting request ends it. */
  if (!queue_.empty()) {
    repeat_active_ = false;
    return StartRequest(now_ms, out);
  }
  if (RepeatDue(now_ms)) {
    /* Period already over (sound longer than repeat_ms): start the next iteration
       right away instead of waiting for the idle poll. */
    return StartRepeat(now_ms, out);
  }
  return PlaybackStep::NONE;
}

PlaybackStep PlaybackSequencer::OnTick(uint32_t now_ms, SoundDefinition& out) {
  if (!repeat_active_) {
    return TakeNext(now_ms, out);
  }

  /* A waiting request still wins over the repetition. */
  if (!queue_.empty()) {
    repeat_active_ = false;
    return StartRequest(now_ms, out);
  }
  if (RepeatDue(now_ms)) {
    return StartRepeat(now_ms, out);
  }
  return PlaybackStep::NONE;
}

void PlaybackSequencer::Stop() {
  queue_.clear();
  repeat_active_ = false;
  pause_active_ = false;
  pause_ms_ = 0U;
}

void PlaybackSequencer::CancelRepeat() {
  repeat_active_ = false;
}

void PlaybackSequencer::Preempt() {
  /* The queue itself is already cleared by Enqueue(preempt). The new request replaces
     the repetition and must not inherit an owed pause from the sound it cuts off. */
  repeat_active_ = false;
  pause_active_ = false;
  pause_ms_ = 0U;
}

uint32_t PlaybackSequencer::TimeUntilRepeat(uint32_t now_ms) const {
  if (!repeat_active_) return UINT32_MAX;
  const uint32_t due = repeat_start_ms_ + current_.repeat_ms;
  if (time_reached(now_ms, due)) return 0U;
  return due - now_ms;
}

uint32_t PlaybackSequencer::TimeUntilPause(uint32_t now_ms) const {
  if (!pause_active_) return 0U;
  const uint32_t due = pause_start_ms_ + pause_ms_;
  if (time_reached(now_ms, due)) return 0U;
  return due - now_ms;
}

}  // namespace xbot::driver::sound
