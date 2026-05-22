#ifndef REMOTE_GPIO_SERVICE_HPP
#define REMOTE_GPIO_SERVICE_HPP

#include <etl/vector.h>
#include <hal.h>
#include <lwjson/lwjson.h>

#include <RemoteGPIOServiceBase.hpp>
#include <services/service_ext.hpp>

class RemoteGPIOService : public RemoteGPIOServiceBase {
 public:
  explicit RemoteGPIOService(uint16_t service_id) : RemoteGPIOServiceBase(service_id, wa, sizeof(wa)) {
  }

 public:
  static constexpr uint8_t kMaxGPIOs = 16;
  static constexpr uint8_t kMaxI2CBuses = 4;
  static constexpr uint8_t kI2CReadBufferSize = 64;

  struct GpioPin {
    uint8_t id;
    ioline_t line;
    bool is_output;
    uint8_t default_value;
    bool subscribed;
    bool periodic;
    uint8_t last_value;
  };

  struct I2CBus {
    uint8_t id;
    I2CDriver* driver;
  };

 private:
  etl::vector<GpioPin, kMaxGPIOs> gpios_{};
  etl::vector<I2CBus, kMaxI2CBuses> i2c_buses_{};
  uint32_t last_periodic_us_ = 0;

  bool OnRegisterGPIOConfigsChanged(const void* data, size_t length) override;
  bool OnStart() override;
  void OnStop() override;
  uint32_t OnLoop(uint32_t now_micros, uint32_t last_tick_micros) override;

  void RPCReadGPIO(uint16_t call_id, uint8_t GPIOID) override;
  void RPCWriteGPIO(uint16_t call_id, uint8_t GPIOID, uint8_t Value) override;
  void RPCSubscribeGPIO(uint16_t call_id, uint8_t GPIOID, uint8_t Periodic) override;
  void RPCUnsubscribeGPIO(uint16_t call_id, uint8_t GPIOID) override;
  void RPCUnsubscribeAll(uint16_t call_id) override;
  void RPCI2cTransmit(uint16_t call_id, uint8_t BusID, uint8_t Address, const uint8_t* Data, uint32_t DataLen) override;
  void RPCI2cReceive(uint16_t call_id, uint8_t BusID, uint8_t Address, uint8_t Count, uint8_t* data,
                     uint16_t* response_length) override;
  void RPCI2cTransmitReceive(uint16_t call_id, uint8_t BusID, uint8_t Address, const uint8_t* TxData,
                             uint32_t TxDataLen, uint8_t RxCount, uint8_t* data, uint16_t* response_length) override;

  GpioPin* FindGpio(uint8_t id);
  I2CBus* FindBus(uint8_t id);
  void EmitGpioEvent(uint8_t gpio_id, uint8_t old_val, uint8_t new_val, uint8_t flags);
  bool HasAnyPeriodicSubscription() const;
  void SetUpHardware();
  void TearDownHardware();
  void ClearSubscriptions();
  bool ConfigJsonCallback(lwjson_stream_parser_t* jsp, lwjson_stream_type_t type, void* data);

  THD_WORKING_AREA(wa, 3072){};
};

#endif  // REMOTE_GPIO_SERVICE_HPP
