#include "services.hpp"

#include "../services/service_ids.h"

EmergencyService emergency_service{xbot::service_ids::EMERGENCY};
#ifdef PWM_DIFF_DRIVE
PWMDiffDriveService diff_drive{xbot::service_ids::DIFF_DRIVE};
#else
DiffDriveService diff_drive{xbot::service_ids::DIFF_DRIVE};
#endif
MowerService mower_service{xbot::service_ids::MOWER};
ImuService imu_service{xbot::service_ids::IMU};
PowerService power_service{xbot::service_ids::POWER};
GpsService gps_service{xbot::service_ids::GPS};
