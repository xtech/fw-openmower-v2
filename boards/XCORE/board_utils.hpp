#ifndef BOARD_UTILS_HPP
#define BOARD_UTILS_HPP

#include <ch.h>
#include <hal.h>

ioline_t GetIoLineByName(const char* name);
UARTDriver* GetUARTDriverByIndex(uint8_t index);

#endif  // BOARD_UTILS_HPP
