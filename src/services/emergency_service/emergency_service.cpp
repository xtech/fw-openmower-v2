//
// Created by clemens on 26.07.24.
//

#include "emergency_service.hpp"

#include "../../globals.hpp"
#include "robot.hpp"

void EmergencyService::OnCreate() {
  // Set low-level ermergency GPIOs to input mode
  auto [sensors, count] = Robot::Emergency::getSensors();
  for (size_t i = 0; i < count; ++i) {
    palSetLineMode(sensors[i].line, PAL_MODE_INPUT);
  }
}

bool EmergencyService::OnStart() {
  emergency_reason = "Boot";
  // set the emergency and notify services
  UpdateMowerStatus([](MowerStatus& mower_status) { mower_status.emergency_latch = true; });
  chEvtBroadcastFlags(&mower_events, MowerEvents::EMERGENCY_CHANGED);
  return true;
}

void EmergencyService::OnStop() {
  emergency_reason = "Stopped";
  // set the emergency and notify services
  UpdateMowerStatus([](MowerStatus& mower_status) { mower_status.emergency_latch = true; });
  chEvtBroadcastFlags(&mower_events, MowerEvents::EMERGENCY_CHANGED);
}

void EmergencyService::tick() {
  // Get the current emergency state
  MowerStatus mower_status = GetMowerStatus();
  bool new_emergency = false;

  // Check for new emergencies only if not already latched
  // Reasoning is that we want to keep the original reason and not overwrite with new emergencies
  if (!mower_status.emergency_latch) {
    // Check heartbeat timeout
    if (chVTTimeElapsedSinceX(last_clear_emergency_message_) > TIME_S2I(1)) {
      emergency_reason = "Timeout";
      new_emergency = true;
    } else if (const auto triggered_sensors = getTriggeredSensors(); !triggered_sensors.empty()) {
      emergency_reason = triggered_sensors;
      new_emergency = true;
    }

    // Handle new emergency (broadcast + update status)
    if (new_emergency) {
      mower_status = UpdateMowerStatus([](MowerStatus& mower_status) { mower_status.emergency_latch = true; });
      chEvtBroadcastFlags(&mower_events, MowerEvents::EMERGENCY_CHANGED);
    }
  }

  // Send status updates
  StartTransaction();
  SendEmergencyActive(mower_status.emergency_active);
  SendEmergencyLatch(mower_status.emergency_latch);
  SendEmergencyReason(emergency_reason.c_str(), emergency_reason.length());
  CommitTransaction();
}

void EmergencyService::OnSetEmergencyChanged(const uint8_t& new_value) {
  if (new_value) {
    emergency_reason = "High Level Emergency";
    // set the emergency and notify services
    UpdateMowerStatus([](MowerStatus& mower_status) { mower_status.emergency_latch = true; });
    chEvtBroadcastFlags(&mower_events, MowerEvents::EMERGENCY_CHANGED);
  } else {
    // Get the current emergency state
    MowerStatus mower_status = GetMowerStatus();

    // want to reset emergency, but only do it, if no emergency exists right now
    if (!mower_status.emergency_active) {
      // clear the emergency and notify services
      UpdateMowerStatus([](MowerStatus& mower_status) { mower_status.emergency_latch = false; });
      chEvtBroadcastFlags(&mower_events, MowerEvents::EMERGENCY_CHANGED);
      emergency_reason = "None";
      last_clear_emergency_message_ = chVTGetSystemTime();
    }
  }
}

bool isEmergencyTimeout(systime_t start, uint32_t timeout_ms) {
  return (start != 0) && (chVTTimeElapsedSinceX(start) >= TIME_MS2I(timeout_ms));
}

etl::string<EmergencyService::MAX_EMERGENCY_REASON_LEN> EmergencyService::getTriggeredSensors() {
  using namespace Robot::Emergency;

  etl::string<MAX_EMERGENCY_REASON_LEN> result;  // Temp. buffer for (concatenated) triggered sensors string
  bool stop_pressed = false;
  bool emergency = false;
  int num_lifted = 0;

  // Check all emergency sensors
  auto [sensors, count] = Robot::Emergency::getSensors();
  for (size_t i = 0; i < count; ++i) {
    const auto& s = sensors[i];                // Alias for easier access
    if (palReadLine(s.line) ^ s.active_low) {  // Sensor triggered
      switch (s.type) {
        case SensorType::WHEEL: num_lifted++; break;
        case SensorType::BUTTON: stop_pressed = true; break;
      }
      // Append sensor name to result
      if (!result.empty() && result.available() >= 2) {
        result.append(", ");
      }
      if (result.available() >= s.name.length()) {
        result.append(s.name.data());
      } else {
        result.append("...");  // Get truncated by etl::string if lomger than available space
        continue;  // Yes, "continue" here and not "break", because we want to skip only the string-append here, and
                   // not sensor readings
      }
    }
  }

  // Handle emergency "Stop" button
  if (stop_pressed) {
    if (button_emergency_started_ == 0) {
      button_emergency_started_ = chVTGetSystemTime();  // Just pressed, store the timestamp for debouncing
    } else {
      button_emergency_started_ = 0;  // Not pressed, reset the time
    }
  }

  // Handle lifted (>=2 wheels are lifted)
  if (num_lifted >= 2) {
    if (lift_emergency_started_ == 0)
      lift_emergency_started_ = chVTGetSystemTime();  // If we just lifted, store the timestamp
  } else {
    lift_emergency_started_ = 0;  // Not lifted, reset the time
  }

  // Handle tilted (at least one wheel is lifted)
  if (num_lifted >= 1) {
    if (tilt_emergency_started_ == 0)
      tilt_emergency_started_ = chVTGetSystemTime();  // If we just tilted, store the timestamp
  } else {
    tilt_emergency_started_ = 0;  // Not tilted, reset the time
  }

  // Evaluate emergencies
  emergency |= stop_pressed && isEmergencyTimeout(button_emergency_started_, BUTTON_EMERGENCY_MILLIS);
  emergency |= (num_lifted >= 2) && isEmergencyTimeout(lift_emergency_started_, getLiftPeriod());
  emergency |= (num_lifted >= 1) && isEmergencyTimeout(tilt_emergency_started_, getTiltPeriod());

  return emergency ? result : "";
}
