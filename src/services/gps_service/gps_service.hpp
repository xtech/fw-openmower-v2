#ifndef GPS_SERVICE_HPP
#define GPS_SERVICE_HPP

#include <drivers/gps/gps_driver.h>

#include <cstring>
#include <globals.hpp>

#include "GpsServiceBase.hpp"
#include "debug/debug_tcp_interface.hpp"

using namespace xbot::driver::gps;

class GpsService : public GpsServiceBase {
 private:
  // 3072 byte stack: GpsService::OnStart() persists changed GPS settings via
  // robot->SaveGpsSettings() -> VersionedStruct<GPSSettings>::Save(), and that call runs deep
  // inside LittleFS (File::mkdirp -> lfs_mkdir/lfs_dir_alloc/lfs_alloc_scan/lfs_fs_traverse ->
  // lfs_dir_fetch -> lfs_bd_read -> read_flash -> wspiReceive), which needs well above 1.5 kB.
  // Measured on a Sabo board: a repeat save (file already exists) uses 1596 bytes, while the very
  // first save on a fresh/just-formatted filesystem needs at least 1892 bytes and is still
  // descending (lfs_fs_traverse walks the whole filesystem looking for free blocks) -- the
  // FileService, which commits the same LittleFS metadata, measured 3388 bytes.
  // With the earlier 1536 bytes the thread ran into the MPU guard page at the bottom of its
  // working area (PORT_ENABLE_GUARD_PAGES), which raises a MemManage fault: chSysHalt in
  // DEBUG_BUILD, a silent NVIC_SystemReset in release builds. Either way the robot stays
  // unreachable for ROS (no MetaService), so this is a hard boot blocker, not a cosmetic issue.
  // TODO: Once the FileService of the sound2 branch is merged, route all firmware-side LittleFS
  // writes through its thread -- it owns the filesystem and already has a 4096 byte stack (see
  // "fix(file_service): increase stack size and avoid repeated mkdirp"). This stack can then be
  // shrunk back to what GpsService itself needs.
  THD_WORKING_AREA(wa, 3072){};

 public:
  explicit GpsService(const uint16_t service_id) : GpsServiceBase(service_id, wa, sizeof(wa)) {
  }

  const GpsDriver::GpsState& GetGpsState() const {
    return gps_driver_ ? gps_driver_->GetGpsState() : empty_gps_state_;
  }

  bool IsGpsStateValid() const {
    return gps_driver_ ? gps_driver_->IsGpsStateValid() : false;
  }

  /**
   * @brief Get seconds since last RTCM packet was received
   * @return Seconds since last RTCM packet, 0 if no data received yet
   */
  uint32_t GetSecondsSinceLastRtcmPacket() const;

  /**
   * @brief Load and start GPS driver instance.
   * Allows initializing the GPS driver before service OnStart(), enabling
   * immediate GPS functionality without waiting for ROS configuration.
   */
  bool LoadAndStartGpsDriver(ProtocolType protocol_type, uint8_t uart, uint32_t baudrate);

 protected:
  bool OnStart() override;

  void OnRTCMChanged(const uint8_t* new_value, uint32_t length) override;

 private:
  GpsDriver* gps_driver_ = nullptr;
  DebugTCPInterface debug_interface_{10000, nullptr};

  // Keep track of the configuration used by the gps_driver_ loaded (initial values don't matter)
  // ATTN: Might become problematic as soon as GpsServiceBase.Uart defaults to != 0
  int used_port_index_ = 0;

  // NTRIP statistics - timestamp of last received RTCM packet
  uint32_t last_ntrip_time_ = 0;

  // Empty GPS state for fallback when no driver is available
  GpsDriver::GpsState empty_gps_state_{};

  void GpsStateCallback(const GpsDriver::GpsState& state);
};

#endif
