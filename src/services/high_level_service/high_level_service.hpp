#ifndef HIGH_LEVEL_SERVICE_HPP
#define HIGH_LEVEL_SERVICE_HPP

#include <etl/string.h>

#include <HighLevelServiceBase.hpp>
#include <xbot-service/Lock.hpp>

using namespace xbot::service;

class HighLevelService : public HighLevelServiceBase {
 private:
  THD_WORKING_AREA(wa, 1024){};

 public:
  explicit HighLevelService(uint16_t service_id) : HighLevelServiceBase(service_id, wa, sizeof(wa)) {
  }

  HighLevelStatus GetStateId() {
    xbot::service::Lock lk{&mtx_};
    return state_id_;
  }

  etl::string<100> GetStateName() {
    xbot::service::Lock lk{&mtx_};
    return state_name_;
  }

  etl::string<100> GetSubStateName() {
    xbot::service::Lock lk{&mtx_};
    return sub_state_name_;
  }

  float GetGpsQuality() {
    xbot::service::Lock lk{&mtx_};
    return gps_quality_;
  }

  int16_t GetCurrentArea() {
    xbot::service::Lock lk{&mtx_};
    return current_area_;
  }

  int16_t GetCurrentPath() {
    xbot::service::Lock lk{&mtx_};
    return current_path_;
  }

  int16_t GetCurrentPathIndex() {
    xbot::service::Lock lk{&mtx_};
    return current_path_index_;
  }

  void SetCallback(const etl::delegate<void()>& callback) {
    xbot::service::Lock lk{&mtx_};
    state_changed_callback_ = callback;
  }

 private:
  MUTEX_DECL(mtx_);

  HighLevelStatus state_id_ = HighLevelStatus::UNKNOWN;
  etl::string<100> state_name_{};
  etl::string<100> sub_state_name_{};
  float gps_quality_ = 0;
  int16_t current_area_ = 0;
  int16_t current_path_ = 0;
  int16_t current_path_index_ = 0;

  etl::delegate<void()> state_changed_callback_{};

  void OnStateIDChanged(const HighLevelStatus& new_value) override;
  void OnStateNameChanged(const char* new_value, uint32_t length) override;
  void OnSubStateNameChanged(const char* new_value, uint32_t length) override;
  void OnGpsQualityChanged(const float& new_value) override;
  void OnCurrentAreaChanged(const int16_t& new_value) override;
  void OnCurrentPathChanged(const int16_t& new_value) override;
  void OnCurrentPathIndexChanged(const int16_t& new_value) override;
  void OnTransactionEnd() override;
};

#endif  // HIGH_LEVEL_SERVICE_HPP
