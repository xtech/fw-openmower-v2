#include "services.hpp"

#include "../services/service_ids.h"
#include "globals.hpp"

EmergencyService emergency_service{xbot::service_ids::EMERGENCY};
DiffDriveService diff_drive{xbot::service_ids::DIFF_DRIVE};
MowerService mower_service{xbot::service_ids::MOWER};
ImuService imu_service{xbot::service_ids::IMU};
PowerService power_service{xbot::service_ids::POWER};
GpsService gps_service{xbot::service_ids::GPS};
MowerUiService mower_ui_service{xbot::service_ids::MOWER_UI};

void StartServices() {
#define START_IF_NEEDED(service, id)                \
  if (robot->NeedsService(xbot::service_ids::id)) { \
    service.start();                                \
  }

  START_IF_NEEDED(emergency_service, EMERGENCY)
  START_IF_NEEDED(imu_service, IMU)
  START_IF_NEEDED(power_service, POWER)
  START_IF_NEEDED(diff_drive, DIFF_DRIVE)
  START_IF_NEEDED(mower_service, MOWER)
  START_IF_NEEDED(gps_service, GPS)
}
