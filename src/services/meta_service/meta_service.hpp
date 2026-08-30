#ifndef META_SERVICE_HPP
#define META_SERVICE_HPP

#include <etl/string_view.h>

#include <MetaServiceBase.hpp>

// Bump when firmware and ROS side are no longer compatible.
// ROS refuses to start if its copy of this constant doesn't match.
static constexpr uint16_t kFirmwareMajorVersion = 1;

class MetaService : public MetaServiceBase {
 public:
  explicit MetaService(uint16_t service_id) : MetaServiceBase(service_id, wa, sizeof(wa)) {
  }

  // Thread-safe: acquire load on the generated .valid field.
  bool HasRobotFirmware() const {
    return __atomic_load_n(&RobotFirmware.valid, __ATOMIC_ACQUIRE);
  }

  // Valid only after HasRobotFirmware() returns true.
  etl::string_view GetRobotFirmware() const {
    return etl::string_view(RobotFirmware.value, RobotFirmware.length);
  }

 protected:
  void RPCGetFirmwareVersion(uint16_t call_id, char* data, uint16_t* response_length) override;
  void RPCGetMajorVersion(uint16_t call_id) override;

 private:
  // 1024 bytes: the previous 512-byte stack overflowed inside newlib snprintf()
  // (remote_logger) while handling SendConfigurationRequest, triggering a
  // MemManage fault in __port_irq_epilogue.
  THD_WORKING_AREA(wa, 1024){};
};

#endif  // META_SERVICE_HPP
