//
// Created by clemens on 26.07.24.
//

#ifndef EMERGENCY_SERVICE_HPP
#define EMERGENCY_SERVICE_HPP

#include <etl/string.h>
#include <etl/vector.h>

#include <EmergencyServiceBase.hpp>

#include "globals.hpp"

using namespace xbot::service;

class EmergencyService : public EmergencyServiceBase {
 private:
  THD_WORKING_AREA(wa, 1024){};

 public:
  explicit EmergencyService(uint16_t service_id) : EmergencyServiceBase(service_id, wa, sizeof(wa)) {
  }

  uint16_t GetEmergencyReasons();
  uint32_t CheckInputs(uint32_t now);

  void RequireService(ServiceExt* svc);

  /**
   * @brief Apply @p add / @p clear to the emergency reasons and report the audio feedback.
   *
   * @return Bitmask of the reasons that actually changed (0 when nothing changed), so a
   *         caller can react to a specific transition — see CheckTimeouts().
   */
  uint16_t UpdateEmergency(uint16_t add, uint16_t clear = 0);

 protected:
  void OnStop() override;
  uint32_t OnLoop(uint32_t now_micros, uint32_t last_tick_micros) override;
  void OnHighLevelEmergencyChanged(const uint16_t* new_value, uint32_t length) override;

 private:
  uint32_t CheckTimeouts(uint32_t now);
  uint32_t CheckRequiredServices();
  void SendStatus();

  /**
   * Heartbeat timeout of the high level (ROS) link — the high level heartbeats every 0.5 s
   * (mower_comms.cpp), so two missed beats mean the link is gone.  Drives
   * EmergencyReason::TIMEOUT_HIGH_LEVEL, which the UI shows as a status and which is
   * reported to the high level, so keep it tight.
   */
  static constexpr uint32_t kHighLevelTimeoutUs = 1'000'000;

  /**
   * @brief How long a link loss has to last *on top of* kHighLevelTimeoutUs before it is
   *        announced (SoundId::ROS_DISCONNECTED).
   *
   * The status bit above may flap on a hiccup, a sound may not: without this extra delay a
   * 1.1 s gap would play "disconnected" and 100 ms later "connected" again.
   */
  static constexpr uint32_t kLinkLostDelayUs = 2'000'000;

  /**
   * @brief Reasons that get the EMERGENCY alert tone.
   *
   * The real dangers: the physical conditions from the input service (their configured
   * `emergency_reason` masks carry EmergencyReason::LATCH together with the condition
   * bit, and only once the input's delay elapsed — see InputService::GetEmergencyReasons)
   * and the mower RPM cutoffs (MowerService / MowingBehavior).  Infrastructure/state
   * reasons (TIMEOUT_*, SERVICE_NOT_READY, HIGH_LEVEL, ...) stay silent: the link state
   * has its own ROS_CONNECTED/ROS_DISCONNECTED sounds, and the high level's initial
   * latched state at connect is normal operation, not an alert.
   */
  static constexpr uint16_t kAudibleReasons =
      EmergencyReason::STOP | EmergencyReason::LIFT | EmergencyReason::LIFT_MULTIPLE | EmergencyReason::COLLISION |
      EmergencyReason::COLLISION_MULTIPLE | EmergencyReason::MOWER_RPM_LIMIT | EmergencyReason::MOWER_RPM_TIMEOUT;

  /** What was last announced for the high level link. */
  enum class LinkAnnounce : uint8_t {
    NONE,       ///< Nothing yet: the high level never talked to us
    CONNECTED,  ///< ROS (high level) is here
    LOST,       ///< ROS went away and we already said so
  };

  /**
   * @brief Announce the high level (ROS) link state, with a debounce.
   *
   * Called from CheckTimeouts() with the changed reason bits and the age of the last
   * high level message.  A link loss is only announced after it lasted for
   * kLinkLostDelayUs (a one second heartbeat timeout alone would flap on every hiccup),
   * a reconnect is announced right away — but only if a loss was announced before or
   * this is the very first connect.
   *
   * @param block_time Clamped so the service loop wakes up when a pending announcement
   *                   becomes due (TimeoutReached() stops clamping once it timed out).
   */
  void AnnounceHighLevelLink(uint16_t changed_reasons, uint32_t heartbeat_age_us, uint32_t& block_time);

  ServiceSchedule status_schedule_{*this, 1'000'000,
                                   XBOT_FUNCTION_FOR_METHOD(EmergencyService, &EmergencyService::SendStatus, this)};

  MUTEX_DECL(mtx_);

  uint16_t reasons_ = EmergencyReason::TIMEOUT_INPUTS | EmergencyReason::TIMEOUT_HIGH_LEVEL;
  uint32_t last_high_level_emergency_message_ = 0;
  LinkAnnounce link_announced_ = LinkAnnounce::NONE;

  /**
   * @brief Whether the EMERGENCY tone was played for the current episode.
   *
   * One alert per emergency episode — new dangers do not re-trigger it
   */
  bool emergency_announced_ = false;

  etl::vector<ServiceExt*, 16> required_services_{};
};

#endif  // EMERGENCY_SERVICE_HPP
