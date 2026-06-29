#ifndef META_SERVICE_HPP
#define META_SERVICE_HPP

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
  const char* GetRobotFirmware() const {
    return RobotFirmware.value;
  }

 protected:
  void RPCGetFirmwareVersion(uint16_t call_id, char* data, uint16_t* response_length) override;
  void RPCGetMajorVersion(uint16_t call_id) override;

 private:
  THD_WORKING_AREA(wa, 512){};
};

#endif  // META_SERVICE_HPP
