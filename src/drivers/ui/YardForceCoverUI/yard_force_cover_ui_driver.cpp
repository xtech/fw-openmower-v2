#include "yard_force_cover_ui_driver.hpp"

#include <sys/unistd.h>
#include <ulog.h>

#include <json_stream.hpp>
#include <services.hpp>

#include "COBS.h"
#include "ui_board.h"

#define IS_BIT_SET(x, bit) ((x & (1 << bit)) != 0)

static constexpr uint8_t EVT_PACKET_RECEIVED = 1;

void YardForceCoverUIDriver::Start(UARTDriver *uart) {
  if (uart == nullptr) {
    chDbgAssert(uart != nullptr, "UART cannot be null");
    return;
  }
  if (thread_ != nullptr) {
    ULOG_ERROR("Started YardForce CoverUI Driver twice!");
    return;
  }

  // Init the UART
  this->uart_ = uart;
  uart_config_.speed = 115200;
  uart_config_.context = this;
  uart_config_.rxchar_cb = YardForceCoverUIDriver::UartRxChar;
  if (uartStart(uart, &uart_config_) != MSG_OK) {
    ULOG_ERROR("Error starting UART");
    return;
  }

  thread_ = chThdCreateStatic(&wa_, sizeof(wa_), NORMALPRIO, ThreadHelper, this);
#ifdef USE_SEGGER_SYSTEMVIEW
  processing_thread_->name = "YardForceUIDriver";
#endif
}

void YardForceCoverUIDriver::ThreadHelper(void *instance) {
  auto *i = static_cast<YardForceCoverUIDriver *>(instance);
  i->ThreadFunc();
}
void YardForceCoverUIDriver::UartRxChar(UARTDriver *driver, uint16_t data) {
  YardForceCoverUIDriver *instance = reinterpret_cast<const UARTConfigEx *>(driver->config)->context;
  chDbgAssert(instance != nullptr, "instance cannot be null!");
  chSysLockFromISR();
  if (instance->processing_) {
    chSysUnlockFromISR();
    return;
  }
  instance->buffer_[instance->buffer_fill_++] = data;
  if (data == 0) {
    chEvtSignalI(instance->thread_, EVT_PACKET_RECEIVED);
  }
  chSysUnlockFromISR();
}
void YardForceCoverUIDriver::ThreadFunc() {
  while (true) {
    bool timeout = chEvtWaitAnyTimeout(EVT_PACKET_RECEIVED, TIME_S2I(1)) == 0;
    if (timeout) {
      if (!board_found_) {
        // Try to find the board
        RequestFWVersion();
      } else {
        UpdateUILeds();
      }

      continue;
    }
    chSysLock();
    // Forbid packet reception
    processing_ = true;
    chSysUnlock();
    if (buffer_fill_ > 0) {
      ProcessPacket();
    }
    buffer_fill_ = 0;
    chSysLock();
    // Allow packet reception
    processing_ = false;
    chSysUnlock();
  }
}

void YardForceCoverUIDriver::UpdateUILeds() {
  // Show Info Docking LED
  msg_set_leds leds_message{};
  leds_message.type = Set_LEDs;

  float charging_current = power_service.GetChargeCurrent();
  float adapter_volts = power_service.GetAdapterVolts();
  float battery_percent = power_service.GetBatteryPercent();
  float gps_quality_percent = high_level_service.GetGpsQuality();
  const auto high_level_state = high_level_service.GetHighLevelState();
  bool emergency = emergency_service.GetEmergency();

  if ((charging_current > 0.80f) && (adapter_volts > 10.0f))
    setLed(leds_message, LED_CHARGING, LED_blink_fast);
  else if ((charging_current <= 0.80f) && (charging_current >= 0.15f) && (adapter_volts > 20.0f))
    setLed(leds_message, LED_CHARGING, LED_blink_slow);
  else if ((charging_current < 0.15f) && (adapter_volts > 20.0f))
    setLed(leds_message, LED_CHARGING, LED_on);
  else
    setLed(leds_message, LED_CHARGING, LED_off);

  // Show Info Battery state
  if (battery_percent >= 0.1)
    setLed(leds_message, LED_BATTERY_LOW, LED_off);
  else
    setLed(leds_message, LED_BATTERY_LOW, LED_on);

  if (adapter_volts < 10.0f)  // activate only when undocked
  {
    // use the first LED row as bargraph
    setBars7(leds_message, battery_percent);
    if (gps_quality_percent == 0) {
      // if quality is 0, flash all LEDs to notify the user to calibrate.
      setBars4(leds_message, -1.0);
    } else {
      setBars4(leds_message, gps_quality_percent);
    }
  } else {
    setBars7(leds_message, 0);
    setBars4(leds_message, 0);
  }

  if (gps_quality_percent < 0.25) {
    setLed(leds_message, LED_POOR_GPS, LED_on);
  } else if (gps_quality_percent < 0.50) {
    setLed(leds_message, LED_POOR_GPS, LED_blink_fast);
  } else if (gps_quality_percent < 0.75) {
    setLed(leds_message, LED_POOR_GPS, LED_blink_slow);
  } else {
    setLed(leds_message, LED_POOR_GPS, LED_off);
  }

  // Let S1 show if ros is connected and which state it's in
  switch (high_level_state) {
    case HighLevelService::HighLevelState::MODE_UNKNOWN:
      setLed(leds_message, LED_S1, LED_off);
      setLed(leds_message, LED_S2, LED_off);
      break;
    case HighLevelService::HighLevelState::MODE_IDLE:
      setLed(leds_message, LED_S1, LED_on);
      setLed(leds_message, LED_S2, LED_off);
      break;
    case HighLevelService::HighLevelState::MODE_AUTONOMOUS_MOWING:
      setLed(leds_message, LED_S1, LED_blink_slow);
      setLed(leds_message, LED_S2, LED_off);
      break;
    case HighLevelService::HighLevelState::MODE_AUTONOMOUS_DOCKING:
      setLed(leds_message, LED_S1, LED_blink_slow);
      setLed(leds_message, LED_S2, LED_blink_slow);
      break;
    case HighLevelService::HighLevelState::MODE_AUTONOMOUS_UNDOCKING:
      setLed(leds_message, LED_S1, LED_blink_slow);
      setLed(leds_message, LED_S2, LED_blink_fast);
      break;
    case HighLevelService::HighLevelState::MODE_RECORDING_OUTLINE:
      setLed(leds_message, LED_S1, LED_blink_fast);
      setLed(leds_message, LED_S2, LED_blink_slow);
      break;
    case HighLevelService::HighLevelState::MODE_RECORDING_OBSTACLE:
      setLed(leds_message, LED_S1, LED_blink_fast);
      setLed(leds_message, LED_S2, LED_blink_fast);
      break;
    default:
      setLed(leds_message, LED_S1, LED_blink_fast);
      setLed(leds_message, LED_S2, LED_on);
      break;
  }

  // Show Info mower lifted or stop button pressed
  if (emergency) {
    setLed(leds_message, LED_MOWER_LIFTED, LED_blink_fast);
  } else {
    setLed(leds_message, LED_MOWER_LIFTED, LED_off);
  }

  sendUIMessage(&leds_message, sizeof(leds_message));
}

void YardForceCoverUIDriver::ProcessPacket() {
  if (buffer_fill_ == 0) return;
  uint16_t size = COBS::decode(const_cast<uint8_t *>(buffer_), buffer_fill_ - 1, encode_decode_buf_);

  auto *crc_pointer = reinterpret_cast<uint16_t *>(encode_decode_buf_ + (size - 2));
  u_int16_t readcrc = *crc_pointer;

  // check structure size
  if (size < 4) {
    return;
  }

  // check the CRC
  CRC16.reset();
  etl::begin(encode_decode_buf_);
  CRC16.add(encode_decode_buf_, encode_decode_buf_ + size - 2);
  uint16_t crc = CRC16.value();

  if (crc != readcrc) return;

  if (encode_decode_buf_[0] == Get_Version && size == sizeof(struct msg_get_version)) {
    board_found_ = true;
  } else if (encode_decode_buf_[0] == Get_Button && size == sizeof(struct msg_event_button)) {
    msg_event_button *msg = (struct msg_event_button *)encode_decode_buf_;
    for (auto &input : input_driver_.Inputs()) {
      if (input.yardforce.button && input.yardforce.id_or_bit == msg->button_id) {
        const bool long_press = msg->press_duration >= 1;
        input.InjectPress(long_press);
        break;
      }
    }
  } else if (encode_decode_buf_[0] == Get_Emergency && size == sizeof(struct msg_event_emergency)) {
    msg_event_emergency *msg = (struct msg_event_emergency *)encode_decode_buf_;
    for (auto &input : input_driver_.Inputs()) {
      if (!input.yardforce.button) {
        input.Update(IS_BIT_SET(msg->state, input.yardforce.id_or_bit));
      }
    }
  } /* else if (encode_decode_buf_[0] == Get_Rain && size == sizeof(struct msg_event_rain)) {
     struct msg_event_rain *msg = (struct msg_event_rain *)encode_decode_buf_;
     stock_ui_rain = (msg->value < llhl_config.rain_threshold);
   } else if (encode_decode_buf_[0] == Get_Subscribe && size == sizeof(struct msg_event_subscribe)) {
     struct msg_event_subscribe *msg = (struct msg_event_subscribe *)encode_decode_buf_;
     ui_topic_bitmask = msg->topic_bitmask;
     ui_interval = msg->interval;
   }*/
}
void YardForceCoverUIDriver::sendUIMessage(void *msg, size_t size) {
  // packages need to be at least 1 byte of type, 1 byte of data and 2 bytes of CRC
  if (size < 4) {
    return;
  }
  auto *data_pointer = static_cast<uint8_t *>(msg);

  // calculate the CRC
  CRC16.reset();
  CRC16.add(data_pointer, data_pointer + size - 2);
  uint16_t crc = CRC16.value();
  data_pointer[size - 1] = (crc >> 8) & 0xFF;
  data_pointer[size - 2] = crc & 0xFF;

  if (COBS::getEncodedBufferSize(size) >= sizeof(encode_decode_buf_)) {
    ULOG_ERROR("out_but_ size too small!");
    return;
  }

  size_t encoded_size = COBS::encode(data_pointer, size, encode_decode_buf_);
  encode_decode_buf_[encoded_size] = 0;
  encoded_size++;
  uartSendFullTimeout(uart_, &encoded_size, encode_decode_buf_, TIME_INFINITE);
}
void YardForceCoverUIDriver::RequestFWVersion() {
  msg_get_version msg{};
  msg.type = Get_Version;
  sendUIMessage(&msg, sizeof(msg));
}
