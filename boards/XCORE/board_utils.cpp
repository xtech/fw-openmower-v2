#include "board_utils.hpp"

UARTDriver* GetUARTDriverByIndex(uint8_t index) {
  switch (index) {
#if STM32_UART_USE_USART1
    case 1: return &UARTD1;
#endif
#if STM32_UART_USE_USART2
    case 2: return &UARTD2;
#endif
#if STM32_UART_USE_USART3
    case 3: return &UARTD3;
#endif
#if STM32_UART_USE_UART4
    case 4: return &UARTD4;
#endif
#if STM32_UART_USE_UART5
    case 5: return &UARTD5;
#endif
#if STM32_UART_USE_USART6
    case 6: return &UARTD6;
#endif
#if STM32_UART_USE_UART7
    case 7: return &UARTD7;
#endif
#if STM32_UART_USE_UART8
    case 8: return &UARTD8;
#endif
#if STM32_UART_USE_UART9
    case 9: return &UARTD9;
#endif
#if STM32_UART_USE_USART10
    case 10: return &UARTD10;
#endif
    default: return nullptr;
  }
}
