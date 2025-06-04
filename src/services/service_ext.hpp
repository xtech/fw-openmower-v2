#ifndef SERVICE_EXT_HPP
#define SERVICE_EXT_HPP

#include <globals.hpp>
#include <xbot-service/Service.hpp>

namespace xbot::service {
class ServiceExt : public Service {
 public:
  explicit ServiceExt(uint16_t service_id, void* stack, size_t stack_size) : Service(service_id, stack, stack_size) {
  }

  void SendEvent(Events::Events id) {
    syssts_t sts = chSysGetStatusAndLockX();
    chEvtSignalI(process_thread_, EVENT_MASK(id));
    chMBPostI(&packet_queue_, reinterpret_cast<msg_t>(nullptr));
    chSysRestoreStatusX(sts);
  }
};
}  // namespace xbot::service

#endif  // SERVICE_EXT_HPP
