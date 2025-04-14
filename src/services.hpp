#ifndef SERVICES_HPP
#define SERVICES_HPP

#include "services/diff_drive_service/diff_drive_service.hpp"
#include "services/emergency_service/emergency_service.hpp"
#include "services/gps_service/gps_service.hpp"
#include "services/imu_service/imu_service.hpp"
#include "services/mower_service/mower_service.hpp"
#include "services/power_service/power_service.hpp"
#include "services/pwm_diff_drive_service/pwm_diff_drive_service.hpp"

extern EmergencyService emergency_service;
#ifdef PWM_DIFF_DRIVE
extern PWMDiffDriveService diff_drive;
#else
extern DiffDriveService diff_drive;
#endif
extern MowerService mower_service;
extern ImuService imu_service;
extern PowerService power_service;
extern GpsService gps_service;

#endif  // SERVICES_HPP
