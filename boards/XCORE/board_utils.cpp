#include "board_utils.hpp"

#include <crc_lookup_map.hpp>

#pragma pack(push, 1)
struct LineParams {
  uint16_t crc;
  uint8_t port : 4;
  uint8_t pad : 4;
};
#pragma pack(pop)

constexpr uint32_t ports[] = {GPIOA_BASE, GPIOB_BASE, GPIOC_BASE, GPIOD_BASE,
                              GPIOE_BASE, GPIOF_BASE, GPIOG_BASE, GPIOH_BASE};

constexpr uint8_t port_idx(char gpio) {
  return gpio - 'A';
}

// From board.h, replace:
//   #define LINE_(\S+)\s+PAL_LINE\(GPIO([A-Z]), (\d+)U\)
//   {crc16("$1"), port_idx('$2'), $3},
constexpr LineParams lines[] = {
    {crc16("AGPIO0"), port_idx('A'), 0},
    {crc16("RMII_REF_CLK"), port_idx('A'), 1},
    {crc16("RMII_MDIO"), port_idx('A'), 2},
    {crc16("AGPIO1"), port_idx('A'), 3},
    {crc16("AGPIO2"), port_idx('A'), 4},
    {crc16("SPI1_SCK"), port_idx('A'), 5},
    {crc16("AGPIO3"), port_idx('A'), 6},
    {crc16("RMII_CRS_DV"), port_idx('A'), 7},
    {crc16("UART7_RX"), port_idx('A'), 8},
    {crc16("UART1_TX"), port_idx('A'), 9},
    {crc16("UART1_RX"), port_idx('A'), 10},
    {crc16("UART4_RX"), port_idx('A'), 11},
    {crc16("UART4_TX"), port_idx('A'), 12},
    {crc16("SWDIO"), port_idx('A'), 13},
    {crc16("SWCLK"), port_idx('A'), 14},
    {crc16("I2S6_WS"), port_idx('A'), 15},
    {crc16("AGPIO4"), port_idx('B'), 0},
    {crc16("AGPIO5"), port_idx('B'), 1},
    {crc16("SPI3_MOSI"), port_idx('B'), 2},
    {crc16("SPI3_SCK"), port_idx('B'), 3},
    {crc16("SPI3_MISO"), port_idx('B'), 4},
    {crc16("I2S6_SDO"), port_idx('B'), 5},
    {crc16("I2C1_SCL"), port_idx('B'), 6},
    {crc16("I2C1_SDA"), port_idx('B'), 7},
    {crc16("FDCAN1_RX"), port_idx('B'), 8},
    {crc16("FDCAN1_TX"), port_idx('B'), 9},
    {crc16("I2C2_SCL"), port_idx('B'), 10},
    {crc16("I2C2_SDA"), port_idx('B'), 11},
    {crc16("RMII_TXD0"), port_idx('B'), 12},
    {crc16("OCTOSPI_IO2"), port_idx('B'), 13},
    {crc16("SPI2_MISO"), port_idx('B'), 14},
    {crc16("SPI2_MOSI"), port_idx('B'), 15},
    {crc16("RMII_MDC"), port_idx('C'), 1},
    {crc16("RMII_RXD0"), port_idx('C'), 4},
    {crc16("RMII_RXD1"), port_idx('C'), 5},
    {crc16("UART6_TX"), port_idx('C'), 6},
    {crc16("UART6_RX"), port_idx('C'), 7},
    {crc16("HIGH_LEVEL_RUN_PG"), port_idx('C'), 13},
    {crc16("HIGH_LEVEL_GLOBAL_EN"), port_idx('C'), 14},
    {crc16("IMU_CS"), port_idx('C'), 15},
    {crc16("SPI2_SCK"), port_idx('D'), 3},
    {crc16("GPIO0"), port_idx('D'), 4},
    {crc16("UART2_TX"), port_idx('D'), 5},
    {crc16("UART2_RX"), port_idx('D'), 6},
    {crc16("SPI1_MOSI"), port_idx('D'), 7},
    {crc16("GPIO8"), port_idx('D'), 8},
    {crc16("GPIO15"), port_idx('D'), 9},
    {crc16("GPIO10"), port_idx('D'), 10},
    {crc16("GPIO1"), port_idx('D'), 11},
    {crc16("I2C4_SCL"), port_idx('D'), 12},
    {crc16("I2C4_SDA"), port_idx('D'), 13},
    {crc16("IMU_INTERRUPT"), port_idx('D'), 15},
    {crc16("UART8_RX"), port_idx('E'), 0},
    {crc16("UART8_TX"), port_idx('E'), 1},
    {crc16("UART10_RX"), port_idx('E'), 2},
    {crc16("UART10_TX"), port_idx('E'), 3},
    {crc16("GPIO2"), port_idx('E'), 4},
    {crc16("GPIO3"), port_idx('E'), 5},
    {crc16("GPIO4"), port_idx('E'), 6},
    {crc16("GPIO21"), port_idx('E'), 7},
    {crc16("GPIO22"), port_idx('E'), 8},
    {crc16("GPIO23"), port_idx('E'), 9},
    {crc16("GPIO9"), port_idx('E'), 10},
    {crc16("GPIO20"), port_idx('E'), 11},
    {crc16("GPIO19"), port_idx('E'), 12},
    {crc16("GPIO18"), port_idx('E'), 13},
    {crc16("GPIO17"), port_idx('E'), 14},
    {crc16("GPIO16"), port_idx('E'), 15},
    {crc16("HEARTBEAT_LED_RED"), port_idx('F'), 0},
    {crc16("HEARTBEAT_LED_GREEN"), port_idx('F'), 1},
    {crc16("HEARTBEAT_LED_BLUE"), port_idx('F'), 2},
    {crc16("RESET_PHY"), port_idx('F'), 3},
    {crc16("OCTOSPI_IO3"), port_idx('F'), 6},
    {crc16("UART7_TX"), port_idx('F'), 7},
    {crc16("OCTOSPI_IO0"), port_idx('F'), 8},
    {crc16("OCTOSPI_IO1"), port_idx('F'), 9},
    {crc16("OCTOSPI_CLK"), port_idx('F'), 10},
    {crc16("STATUS_LED_RED"), port_idx('F'), 11},
    {crc16("STATUS_LED_GREEN"), port_idx('F'), 12},
    {crc16("STATUS_LED"), port_idx('F'), 12},
    {crc16("STATUS_LED_BLUE"), port_idx('F'), 13},
    {crc16("GPIO14"), port_idx('G'), 2},
    {crc16("GPIO5"), port_idx('G'), 3},
    {crc16("GPIO13"), port_idx('G'), 4},
    {crc16("GPIO12"), port_idx('G'), 5},
    {crc16("OCTOSPI_NCS"), port_idx('G'), 6},
    {crc16("GPIO6"), port_idx('G'), 7},
    {crc16("GPIO11"), port_idx('G'), 8},
    {crc16("SPI1_MISO"), port_idx('G'), 9},
    {crc16("GPIO7"), port_idx('G'), 10},
    {crc16("RMII_TX_EN"), port_idx('G'), 11},
    {crc16("I2S6_SDI"), port_idx('G'), 12},
    {crc16("I2S6_CK"), port_idx('G'), 13},
    {crc16("RMII_TXD1"), port_idx('G'), 14},
    {crc16("OSC_IN"), port_idx('H'), 0},
    {crc16("OSC_OUT"), port_idx('H'), 1},
};

static_assert(HasUniqueCrcs(lines), "CRC16 values are not unique");

ioline_t GetIoLineByName(const char* name) {
  auto* line = LookupByName(name, lines);
  return line != nullptr ? PAL_LINE(ports[line->port], line->pad) : PAL_NOLINE;
}

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
