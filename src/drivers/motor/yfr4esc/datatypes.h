#ifndef YFR4ESC_DATATYPES_H
#define YFR4ESC_DATATYPES_H

#include <cstdint>

namespace xbot::driver::motor::yfr4esc {

enum MessageType : uint8_t { STATUS = 1, CONTROL = 2, SETTINGS = 3 };

enum FaultBit : uint8_t {
  UNINITIALIZED = 0,
  WATCHDOG = 1,
  UNDERVOLTAGE = 2,
  OVERVOLTAGE = 3,
  OVERCURRENT = 4,
  OVERTEMP_MOTOR = 5,
  OVERTEMP_PCB = 6,
  INVALID_HALL = 7,
  INTERNAL_ERROR = 8,
  OPEN_LOAD = 9
};

#pragma pack(push, 1)
struct StatusPacket {
  uint8_t message_type;
  uint32_t seq;
  uint8_t fw_version_major;
  uint8_t fw_version_minor;
  double temperature_pcb;   // temperature of printed circuit board (degrees Celsius)
  double current_input;     // input current (ampere)
  double duty_cycle;        // duty cycle (0 to 1)
  bool direction;           // direction CW/CCW
  uint32_t tacho;           // wheel ticks
  uint32_t tacho_absolute;  // wheel ticks absolute
  uint16_t rpm;             // revolutions per minute (of the axis/shaft)
  int32_t fault_code;
  uint16_t crc;
} __attribute__((packed));
#pragma pack(pop)

#pragma pack(push, 1)
struct ControlPacket {
  uint8_t message_type;
  double duty_cycle;  // duty cycle (0 to 1)
  uint16_t crc;
} __attribute__((packed));
#pragma pack(pop)

#pragma pack(push, 1)
struct SettingsPacket {
  uint8_t message_type;
  float motor_current_limit;
  float min_pcb_temp;
  float max_pcb_temp;
  uint16_t crc;
} __attribute__((packed));
#pragma pack(pop)

}  // namespace xbot::driver::motor::yfr4esc

#endif  // YFR4ESC_DATATYPES_H
