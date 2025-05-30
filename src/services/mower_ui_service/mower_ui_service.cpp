//
// Created by clemens on 5/7/25.
//

#include "mower_ui_service.hpp"

void MowerUiService::OnStateIDChanged(const HighLevelStatus &new_value) {
  xbot::service::Lock lk{&mtx_};
  state_id_ = new_value;
}

void MowerUiService::OnStateNameChanged(const char *new_value, uint32_t length) {
  xbot::service::Lock lk{&mtx_};
  if (new_value && length > 0 && length < state_name_.max_size()) {
    state_name_ = new_value;
  }
}

void MowerUiService::OnSubStateNameChanged(const char *new_value, uint32_t length) {
  xbot::service::Lock lk{&mtx_};
  if (new_value && length > 0 && length < sub_state_name_.max_size()) {
    sub_state_name_ = new_value;
  }
}

void MowerUiService::OnGpsQualityChanged(const float &new_value) {
  xbot::service::Lock lk{&mtx_};
  gps_quality_ = new_value;
}

void MowerUiService::OnCurrentAreaChanged(const int16_t &new_value) {
  xbot::service::Lock lk{&mtx_};
  current_area_ = new_value;
}

void MowerUiService::OnCurrentPathChanged(const int16_t &new_value) {
  xbot::service::Lock lk{&mtx_};
  current_path_ = new_value;
}

void MowerUiService::OnCurrentPathIndexChanged(const int16_t &new_value) {
  xbot::service::Lock lk{&mtx_};
  current_path_index_ = new_value;
}
void MowerUiService::OnTransactionEnd() {
  if (state_changed_callback_) {
    state_changed_callback_();
  }
}
