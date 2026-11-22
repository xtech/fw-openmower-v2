//
// Detection of a charger that runs without a battery pack.
//

#ifndef NO_BATTERY_DETECTOR_HPP
#define NO_BATTERY_DETECTOR_HPP

#include <cstdint>

#include "charger.hpp"

using CHARGER_STATUS = ChargerDriver::CHARGER_STATUS;

/**
 * @brief Decides whether a charger runs without a battery pack.
 *
 * Without a pack only the bulk capacitors sit on the battery rail: the charger reaches
 * its termination current right away, terminates, sees the capacitors discharge below
 * the recharge threshold and starts over. The charge state therefore flips between an
 * active phase and DONE roughly once a second, and every consumer (sound, UI, high
 * level) sees those edges as real charge events.
 *
 * A real pack stays in CV until the termination current is reached and then remains in
 * DONE - the next change is a recharge hours later.
 *
 * The detector counts the edges between an active phase and DONE inside a window and
 * requires the battery current to stay near zero. It latches the result until a state
 * holds stable for kClearStableMs, so consumers see one clean status instead of the
 * flap.
 *
 * The thresholds are constants on purpose: tune them here with a real pack on the bench.
 */
class NoBatteryDetector {
 public:
  static constexpr uint32_t kEdgeWindowMs = 3'000U;   // window the edges are counted in
  static constexpr uint32_t kMinEdgesInWindow = 3U;   // active<->DONE edges that declare "no pack"
  static constexpr uint32_t kClearStableMs = 3'000U;  // stable time that clears "no pack"
  static constexpr float kNoCurrentAmps = 0.05f;      // |IBAT| below this: no pack current flows
  static constexpr uint8_t kEdgeHistory = 8U;         // ring buffer for the edge timestamps

  /**
   * @brief Feeds one status sample.
   *
   * @param status              raw charge phase as reported by the charger IC
   * @param charge_current_amps battery current (sign is irrelevant)
   * @param now_ms              monotonic milliseconds, wrap-around safe
   * @return true while a missing pack is assumed
   */
  bool update(CHARGER_STATUS status, float charge_current_amps, uint32_t now_ms) {
    const bool is_active = isActivePhase(status);
    const bool was_active = isActivePhase(last_status_);
    const bool transition =
        (is_active && last_status_ == CHARGER_STATUS::DONE) || (status == CHARGER_STATUS::DONE && was_active);
    const bool no_current = absolute(charge_current_amps) < kNoCurrentAmps;

    if (transition && no_current) {
      addEdge(now_ms);
      last_edge_ms_ = now_ms;
    }
    last_status_ = status;

    pruneEdges(now_ms);

    if ((edge_count_ >= kMinEdgesInWindow) && no_current) {
      no_battery_ = true;
    } else if (no_battery_ && (now_ms - last_edge_ms_) >= kClearStableMs) {
      // The state holds: the pack is back (or the charger is idle/done for good).
      no_battery_ = false;
    }
    return no_battery_;
  }

  [[nodiscard]] bool noBattery() const {
    return no_battery_;
  }

 private:
  static bool isActivePhase(CHARGER_STATUS status) {
    switch (status) {
      case CHARGER_STATUS::TRICKLE:
      case CHARGER_STATUS::PRE_CHARGE:
      case CHARGER_STATUS::CC:
      case CHARGER_STATUS::CV:
      case CHARGER_STATUS::TOP_OFF: return true;
      default: return false;
    }
  }

  static float absolute(float value) {
    return value < 0.0f ? -value : value;
  }

  void addEdge(uint32_t now_ms) {
    edge_times_[next_edge_] = now_ms;
    next_edge_ = static_cast<uint8_t>((next_edge_ + 1U) % kEdgeHistory);
    if (edge_count_ < kEdgeHistory) {
      edge_count_++;
    }
  }

  // Drops the edges that left the window, the ring buffer keeps them in insertion order.
  void pruneEdges(uint32_t now_ms) {
    while (edge_count_ > 0U) {
      const uint8_t oldest = static_cast<uint8_t>((next_edge_ + kEdgeHistory - edge_count_) % kEdgeHistory);
      if ((now_ms - edge_times_[oldest]) < kEdgeWindowMs) {
        break;
      }
      edge_count_--;
    }
  }

  CHARGER_STATUS last_status_ = CHARGER_STATUS::UNKNOWN;
  uint32_t edge_times_[kEdgeHistory] = {};
  uint32_t last_edge_ms_ = 0;
  uint8_t next_edge_ = 0;
  uint8_t edge_count_ = 0;
  bool no_battery_ = false;
};

#endif  // NO_BATTERY_DETECTOR_HPP
