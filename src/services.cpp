#include "services.hpp"

#include "../services/service_ids.h"

EmergencyService emergency_service{xbot::service_ids::EMERGENCY};
DiffDriveService diff_drive{xbot::service_ids::DIFF_DRIVE};
#ifndef NO_MOWER_SERVICE
MowerService mower_service{xbot::service_ids::MOWER};
#endif
ImuService imu_service{xbot::service_ids::IMU};
PowerService power_service{xbot::service_ids::POWER};
GpsService gps_service{xbot::service_ids::GPS};
