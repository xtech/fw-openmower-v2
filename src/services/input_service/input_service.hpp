#ifndef INPUT_SERVICE_HPP
#define INPUT_SERVICE_HPP

#include <InputServiceBase.hpp>

using namespace xbot::service;

class InputService : public InputServiceBase {
 public:
  explicit InputService(uint16_t service_id) : InputServiceBase(service_id, wa, sizeof(wa)) {
  }

 private:
  bool OnStart() override;
  void tick();
  ManagedSchedule tick_schedule_{scheduler_, IsRunning(), 20'000,
                                 XBOT_FUNCTION_FOR_METHOD(InputService, &InputService::tick, this)};

  THD_WORKING_AREA(wa, 1500) {};
};

#endif  // INPUT_SERVICE_HPP
