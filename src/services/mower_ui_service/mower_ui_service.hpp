//
// Created by clemens on 5/7/25.
//

#ifndef OPENMOWER_MOWER_UI_SERVICE_HPP
#define OPENMOWER_MOWER_UI_SERVICE_HPP

#include <MowerUiServiceBase.hpp>

#include "globals.hpp"
#include <etl/string.h>
#include <xbot-service/Lock.hpp>

using namespace xbot::service;

class MowerUiService : public MowerUiServiceBase {
 private:
  THD_WORKING_AREA(wa, 1024) {};

 public:
  explicit MowerUiService(uint16_t service_id) : MowerUiServiceBase(service_id, wa, sizeof(wa)) {
  }
  
  [[nodiscard]] HighLevelStatus getStateId() { 
    xbot::service::Lock lk{&mtx_};
    return state_id_; 
  }
  [[nodiscard]] etl::string<100> getStateName() {
    xbot::service::Lock lk{&mtx_};
    return state_name_;
  }
  [[nodiscard]] etl::string<100> getSubStateName() {
    xbot::service::Lock lk{&mtx_};
    return sub_state_name_;
  }
  [[nodiscard]] float getGpsQuality() {
    xbot::service::Lock lk{&mtx_};
    return gps_quality_;
  }
  [[nodiscard]] int16_t getCurrentArea() {
    xbot::service::Lock lk{&mtx_};
    return current_area_;
  }
  [[nodiscard]] int16_t getCurrentPath() {
    xbot::service::Lock lk{&mtx_};
    return current_path_;
  }
  [[nodiscard]] int16_t getCurrentPathIndex() {
    xbot::service::Lock lk{&mtx_};
    return current_path_index_;
  }

  void SendAction(HighLevelAction action);

  void SetCallback(const etl::delegate<void()> &callback) {
    xbot::service::Lock lk{&mtx_};
    state_changed_callback_ = callback;
  }

 private:
  MUTEX_DECL(mtx_);
  
  HighLevelStatus state_id_ = MowerUiServiceBase::HighLevelStatus::UNKNOWN;
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

#endif  // OPENMOWER_MOWER_UI_SERVICE_HPP
