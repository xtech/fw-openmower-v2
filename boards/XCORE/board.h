/*
    ChibiOS - Copyright (C) 2006..2020 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

/*
 * This file has been automatically generated using ChibiStudio board
 * generator plugin. Do not edit manually.
 */

#ifndef BOARD_H
#define BOARD_H

#include "board_ex.h"

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/*
 * Setup for xcore board.
 */

/*
 * Board identifier.
 */
#define BOARD_XCORE
#define BOARD_NAME                  "xcore"

/*
 * Ethernet PHY type.
 */
#define BOARD_PHY_ID                MII_GSW141_ID
#define BOARD_PHY_RMII

/*
 * Board oscillators-related settings.
 * NOTE: LSE not fitted.
 */
#if !defined(STM32_LSECLK)
#define STM32_LSECLK                0U
#endif

#define STM32_LSEDRV                (3U << 3U)

#if !defined(STM32_HSECLK)
#define STM32_HSECLK                25000000U
#endif

/*
 * MCU type as defined in the ST header.
 */
#define STM32H723xx

/*
 * IO pins assignments.
 */
#define GPIOA_AGPIO0                0U
#define GPIOA_RMII_REF_CLK          1U
#define GPIOA_RMII_MDIO             2U
#define GPIOA_AGPIO1                3U
#define GPIOA_AGPIO2                4U
#define GPIOA_SPI1_SCK              5U
#define GPIOA_AGPIO3                6U
#define GPIOA_RMII_CRS_DV           7U
#define GPIOA_UART7_RX              8U
#define GPIOA_UART1_TX              9U
#define GPIOA_UART1_RX              10U
#define GPIOA_UART4_RX              11U
#define GPIOA_UART4_TX              12U
#define GPIOA_SWDIO                 13U
#define GPIOA_SWCLK                 14U
#define GPIOA_I2S6_WS               15U

#define GPIOB_AGPIO4                0U
#define GPIOB_AGPIO5                1U
#define GPIOB_SPI3_MOSI             2U
#define GPIOB_SPI3_SCK              3U
#define GPIOB_SPI3_MISO             4U
#define GPIOB_I2S6_SDO              5U
#define GPIOB_I2C1_SCL              6U
#define GPIOB_I2C1_SDA              7U
#define GPIOB_FDCAN1_RX             8U
#define GPIOB_FDCAN1_TX             9U
#define GPIOB_I2C2_SCL              10U
#define GPIOB_I2C2_SDA              11U
#define GPIOB_RMII_TXD0             12U
#define GPIOB_OCTOSPI_IO2           13U
#define GPIOB_SPI2_MISO             14U
#define GPIOB_SPI2_MOSI             15U

#define GPIOC_PIN0                  0U
#define GPIOC_RMII_MDC              1U
#define GPIOC_PIN2                  2U
#define GPIOC_PIN3                  3U
#define GPIOC_RMII_RXD0             4U
#define GPIOC_RMII_RXD1             5U
#define GPIOC_UART6_TX              6U
#define GPIOC_UART6_RX              7U
#define GPIOC_PIN8                  8U
#define GPIOC_PIN9                  9U
#define GPIOC_PIN10                 10U
#define GPIOC_PIN11                 11U
#define GPIOC_PIN12                 12U
#define GPIOC_HIGH_LEVEL_RUN_PG     13U
#define GPIOC_HIGH_LEVEL_GLOBAL_EN  14U
#define GPIOC_IMU_CS                15U

#define GPIOD_PIN0                  0U
#define GPIOD_PIN1                  1U
#define GPIOD_PIN2                  2U
#define GPIOD_SPI2_SCK              3U
#define GPIOD_GPIO0                 4U
#define GPIOD_UART2_TX              5U
#define GPIOD_UART2_RX              6U
#define GPIOD_SPI1_MOSI             7U
#define GPIOD_GPIO8                 8U
#define GPIOD_GPIO15                9U
#define GPIOD_GPIO10                10U
#define GPIOD_GPIO1                 11U
#define GPIOD_I2C4_SCL              12U
#define GPIOD_I2C4_SDA              13U
#define GPIOD_PIN14                 14U
#define GPIOD_IMU_INTERRUPT         15U

#define GPIOE_UART8_RX              0U
#define GPIOE_UART8_TX              1U
#define GPIOE_UART10_RX             2U
#define GPIOE_UART10_TX             3U
#define GPIOE_GPIO2                 4U
#define GPIOE_GPIO3                 5U
#define GPIOE_GPIO4                 6U
#define GPIOE_GPIO21                7U
#define GPIOE_GPIO22                8U
#define GPIOE_GPIO23                9U
#define GPIOE_GPIO9                 10U
#define GPIOE_GPIO20                11U
#define GPIOE_GPIO19                12U
#define GPIOE_GPIO18                13U
#define GPIOE_GPIO17                14U
#define GPIOE_GPIO16                15U

#define GPIOF_HEARTBEAT_LED_RED     0U
#define GPIOF_HEARTBEAT_LED_GREEN   1U
#define GPIOF_HEARTBEAT_LED_BLUE    2U
#define GPIOF_RESET_PHY             3U
#define GPIOF_PIN4                  4U
#define GPIOF_PIN5                  5U
#define GPIOF_OCTOSPI_IO3           6U
#define GPIOF_UART7_TX              7U
#define GPIOF_OCTOSPI_IO0           8U
#define GPIOF_OCTOSPI_IO1           9U
#define GPIOF_OCTOSPI_CLK           10U
#define GPIOF_STATUS_LED_RED        11U
#define GPIOF_STATUS_LED_GREEN      12U
#define GPIOF_STATUS_LED            12U
#define GPIOF_STATUS_LED_BLUE       13U
#define GPIOF_PIN14                 14U
#define GPIOF_PIN15                 15U

#define GPIOG_PIN0                  0U
#define GPIOG_PIN1                  1U
#define GPIOG_GPIO14                2U
#define GPIOG_GPIO5                 3U
#define GPIOG_GPIO13                4U
#define GPIOG_GPIO12                5U
#define GPIOG_OCTOSPI_NCS           6U
#define GPIOG_GPIO6                 7U
#define GPIOG_GPIO11                8U
#define GPIOG_SPI1_MISO             9U
#define GPIOG_GPIO7                 10U
#define GPIOG_RMII_TX_EN            11U
#define GPIOG_I2S6_SDI              12U
#define GPIOG_I2S6_CK               13U
#define GPIOG_RMII_TXD1             14U
#define GPIOG_PIN15                 15U

#define GPIOH_OSC_IN                0U
#define GPIOH_OSC_OUT               1U
#define GPIOH_PIN2                  2U
#define GPIOH_PIN3                  3U
#define GPIOH_PIN4                  4U
#define GPIOH_PIN5                  5U
#define GPIOH_PIN6                  6U
#define GPIOH_PIN7                  7U
#define GPIOH_PIN8                  8U
#define GPIOH_PIN9                  9U
#define GPIOH_PIN10                 10U
#define GPIOH_PIN11                 11U
#define GPIOH_PIN12                 12U
#define GPIOH_PIN13                 13U
#define GPIOH_PIN14                 14U
#define GPIOH_PIN15                 15U

#define GPIOI_PIN0                  0U
#define GPIOI_PIN1                  1U
#define GPIOI_PIN2                  2U
#define GPIOI_PIN3                  3U
#define GPIOI_PIN4                  4U
#define GPIOI_PIN5                  5U
#define GPIOI_PIN6                  6U
#define GPIOI_PIN7                  7U
#define GPIOI_PIN8                  8U
#define GPIOI_PIN9                  9U
#define GPIOI_PIN10                 10U
#define GPIOI_PIN11                 11U
#define GPIOI_PIN12                 12U
#define GPIOI_PIN13                 13U
#define GPIOI_PIN14                 14U
#define GPIOI_PIN15                 15U

#define GPIOJ_PIN0                  0U
#define GPIOJ_PIN1                  1U
#define GPIOJ_PIN2                  2U
#define GPIOJ_PIN3                  3U
#define GPIOJ_PIN4                  4U
#define GPIOJ_PIN5                  5U
#define GPIOJ_PIN6                  6U
#define GPIOJ_PIN7                  7U
#define GPIOJ_PIN8                  8U
#define GPIOJ_PIN9                  9U
#define GPIOJ_PIN10                 10U
#define GPIOJ_PIN11                 11U
#define GPIOJ_PIN12                 12U
#define GPIOJ_PIN13                 13U
#define GPIOJ_PIN14                 14U
#define GPIOJ_PIN15                 15U

#define GPIOK_PIN0                  0U
#define GPIOK_PIN1                  1U
#define GPIOK_PIN2                  2U
#define GPIOK_PIN3                  3U
#define GPIOK_PIN4                  4U
#define GPIOK_PIN5                  5U
#define GPIOK_PIN6                  6U
#define GPIOK_PIN7                  7U
#define GPIOK_PIN8                  8U
#define GPIOK_PIN9                  9U
#define GPIOK_PIN10                 10U
#define GPIOK_PIN11                 11U
#define GPIOK_PIN12                 12U
#define GPIOK_PIN13                 13U
#define GPIOK_PIN14                 14U
#define GPIOK_PIN15                 15U

/*
 * IO lines assignments.
 */
#define LINE_AGPIO0                 PAL_LINE(GPIOA, 0U)
#define LINE_RMII_REF_CLK           PAL_LINE(GPIOA, 1U)
#define LINE_RMII_MDIO              PAL_LINE(GPIOA, 2U)
#define LINE_AGPIO1                 PAL_LINE(GPIOA, 3U)
#define LINE_AGPIO2                 PAL_LINE(GPIOA, 4U)
#define LINE_SPI1_SCK               PAL_LINE(GPIOA, 5U)
#define LINE_AGPIO3                 PAL_LINE(GPIOA, 6U)
#define LINE_RMII_CRS_DV            PAL_LINE(GPIOA, 7U)
#define LINE_UART7_RX               PAL_LINE(GPIOA, 8U)
#define LINE_UART1_TX               PAL_LINE(GPIOA, 9U)
#define LINE_UART1_RX               PAL_LINE(GPIOA, 10U)
#define LINE_UART4_RX               PAL_LINE(GPIOA, 11U)
#define LINE_UART4_TX               PAL_LINE(GPIOA, 12U)
#define LINE_SWDIO                  PAL_LINE(GPIOA, 13U)
#define LINE_SWCLK                  PAL_LINE(GPIOA, 14U)
#define LINE_I2S6_WS                PAL_LINE(GPIOA, 15U)
#define LINE_AGPIO4                 PAL_LINE(GPIOB, 0U)
#define LINE_AGPIO5                 PAL_LINE(GPIOB, 1U)
#define LINE_SPI3_MOSI              PAL_LINE(GPIOB, 2U)
#define LINE_SPI3_SCK               PAL_LINE(GPIOB, 3U)
#define LINE_SPI3_MISO              PAL_LINE(GPIOB, 4U)
#define LINE_I2S6_SDO               PAL_LINE(GPIOB, 5U)
#define LINE_I2C1_SCL               PAL_LINE(GPIOB, 6U)
#define LINE_I2C1_SDA               PAL_LINE(GPIOB, 7U)
#define LINE_FDCAN1_RX              PAL_LINE(GPIOB, 8U)
#define LINE_FDCAN1_TX              PAL_LINE(GPIOB, 9U)
#define LINE_I2C2_SCL               PAL_LINE(GPIOB, 10U)
#define LINE_I2C2_SDA               PAL_LINE(GPIOB, 11U)
#define LINE_RMII_TXD0              PAL_LINE(GPIOB, 12U)
#define LINE_OCTOSPI_IO2            PAL_LINE(GPIOB, 13U)
#define LINE_SPI2_MISO              PAL_LINE(GPIOB, 14U)
#define LINE_SPI2_MOSI              PAL_LINE(GPIOB, 15U)
#define LINE_RMII_MDC               PAL_LINE(GPIOC, 1U)
#define LINE_RMII_RXD0              PAL_LINE(GPIOC, 4U)
#define LINE_RMII_RXD1              PAL_LINE(GPIOC, 5U)
#define LINE_UART6_TX               PAL_LINE(GPIOC, 6U)
#define LINE_UART6_RX               PAL_LINE(GPIOC, 7U)
#define LINE_HIGH_LEVEL_RUN_PG      PAL_LINE(GPIOC, 13U)
#define LINE_HIGH_LEVEL_GLOBAL_EN   PAL_LINE(GPIOC, 14U)
#define LINE_IMU_CS                 PAL_LINE(GPIOC, 15U)
#define LINE_SPI2_SCK               PAL_LINE(GPIOD, 3U)
#define LINE_GPIO0                  PAL_LINE(GPIOD, 4U)
#define LINE_UART2_TX               PAL_LINE(GPIOD, 5U)
#define LINE_UART2_RX               PAL_LINE(GPIOD, 6U)
#define LINE_SPI1_MOSI              PAL_LINE(GPIOD, 7U)
#define LINE_GPIO8                  PAL_LINE(GPIOD, 8U)
#define LINE_GPIO15                 PAL_LINE(GPIOD, 9U)
#define LINE_GPIO10                 PAL_LINE(GPIOD, 10U)
#define LINE_GPIO1                  PAL_LINE(GPIOD, 11U)
#define LINE_I2C4_SCL               PAL_LINE(GPIOD, 12U)
#define LINE_I2C4_SDA               PAL_LINE(GPIOD, 13U)
#define LINE_IMU_INTERRUPT          PAL_LINE(GPIOD, 15U)
#define LINE_UART8_RX               PAL_LINE(GPIOE, 0U)
#define LINE_UART8_TX               PAL_LINE(GPIOE, 1U)
#define LINE_UART10_RX              PAL_LINE(GPIOE, 2U)
#define LINE_UART10_TX              PAL_LINE(GPIOE, 3U)
#define LINE_GPIO2                  PAL_LINE(GPIOE, 4U)
#define LINE_GPIO3                  PAL_LINE(GPIOE, 5U)
#define LINE_GPIO4                  PAL_LINE(GPIOE, 6U)
#define LINE_GPIO21                 PAL_LINE(GPIOE, 7U)
#define LINE_GPIO22                 PAL_LINE(GPIOE, 8U)
#define LINE_GPIO23                 PAL_LINE(GPIOE, 9U)
#define LINE_GPIO9                  PAL_LINE(GPIOE, 10U)
#define LINE_GPIO20                 PAL_LINE(GPIOE, 11U)
#define LINE_GPIO19                 PAL_LINE(GPIOE, 12U)
#define LINE_GPIO18                 PAL_LINE(GPIOE, 13U)
#define LINE_GPIO17                 PAL_LINE(GPIOE, 14U)
#define LINE_GPIO16                 PAL_LINE(GPIOE, 15U)
#define LINE_HEARTBEAT_LED_RED      PAL_LINE(GPIOF, 0U)
#define LINE_HEARTBEAT_LED_GREEN    PAL_LINE(GPIOF, 1U)
#define LINE_HEARTBEAT_LED_BLUE     PAL_LINE(GPIOF, 2U)
#define LINE_RESET_PHY              PAL_LINE(GPIOF, 3U)
#define LINE_OCTOSPI_IO3            PAL_LINE(GPIOF, 6U)
#define LINE_UART7_TX               PAL_LINE(GPIOF, 7U)
#define LINE_OCTOSPI_IO0            PAL_LINE(GPIOF, 8U)
#define LINE_OCTOSPI_IO1            PAL_LINE(GPIOF, 9U)
#define LINE_OCTOSPI_CLK            PAL_LINE(GPIOF, 10U)
#define LINE_STATUS_LED_RED         PAL_LINE(GPIOF, 11U)
#define LINE_STATUS_LED_GREEN       PAL_LINE(GPIOF, 12U)
#define LINE_STATUS_LED             PAL_LINE(GPIOF, 12U)
#define LINE_STATUS_LED_BLUE        PAL_LINE(GPIOF, 13U)
#define LINE_GPIO14                 PAL_LINE(GPIOG, 2U)
#define LINE_GPIO5                  PAL_LINE(GPIOG, 3U)
#define LINE_GPIO13                 PAL_LINE(GPIOG, 4U)
#define LINE_GPIO12                 PAL_LINE(GPIOG, 5U)
#define LINE_OCTOSPI_NCS            PAL_LINE(GPIOG, 6U)
#define LINE_GPIO6                  PAL_LINE(GPIOG, 7U)
#define LINE_GPIO11                 PAL_LINE(GPIOG, 8U)
#define LINE_SPI1_MISO              PAL_LINE(GPIOG, 9U)
#define LINE_GPIO7                  PAL_LINE(GPIOG, 10U)
#define LINE_RMII_TX_EN             PAL_LINE(GPIOG, 11U)
#define LINE_I2S6_SDI               PAL_LINE(GPIOG, 12U)
#define LINE_I2S6_CK                PAL_LINE(GPIOG, 13U)
#define LINE_RMII_TXD1              PAL_LINE(GPIOG, 14U)
#define LINE_OSC_IN                 PAL_LINE(GPIOH, 0U)
#define LINE_OSC_OUT                PAL_LINE(GPIOH, 1U)

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Driver macros.                                                            */
/*===========================================================================*/

/*
 * I/O ports initial setup, this configuration is established soon after reset
 * in the initialization code.
 * Please refer to the STM32 Reference Manual for details.
 */
#define PIN_MODE_INPUT(n)           (0U << ((n) * 2U))
#define PIN_MODE_OUTPUT(n)          (1U << ((n) * 2U))
#define PIN_MODE_ALTERNATE(n)       (2U << ((n) * 2U))
#define PIN_MODE_ANALOG(n)          (3U << ((n) * 2U))
#define PIN_ODR_LOW(n)              (0U << (n))
#define PIN_ODR_HIGH(n)             (1U << (n))
#define PIN_OTYPE_PUSHPULL(n)       (0U << (n))
#define PIN_OTYPE_OPENDRAIN(n)      (1U << (n))
#define PIN_OSPEED_VERYLOW(n)       (0U << ((n) * 2U))
#define PIN_OSPEED_LOW(n)           (1U << ((n) * 2U))
#define PIN_OSPEED_MEDIUM(n)        (2U << ((n) * 2U))
#define PIN_OSPEED_HIGH(n)          (3U << ((n) * 2U))
#define PIN_PUPDR_FLOATING(n)       (0U << ((n) * 2U))
#define PIN_PUPDR_PULLUP(n)         (1U << ((n) * 2U))
#define PIN_PUPDR_PULLDOWN(n)       (2U << ((n) * 2U))
#define PIN_AFIO_AF(n, v)           ((v) << (((n) % 8U) * 4U))

/*
 * GPIOA setup:
 *
 * PA0  - AGPIO0                    (analog).
 * PA1  - RMII_REF_CLK              (alternate 11).
 * PA2  - RMII_MDIO                 (alternate 11).
 * PA3  - AGPIO1                    (analog).
 * PA4  - AGPIO2                    (analog).
 * PA5  - SPI1_SCK                  (alternate 5).
 * PA6  - AGPIO3                    (analog).
 * PA7  - RMII_CRS_DV               (alternate 11).
 * PA8  - UART7_RX                  (alternate 11).
 * PA9  - UART1_TX                  (alternate 7).
 * PA10 - UART1_RX                  (alternate 7).
 * PA11 - UART4_RX                  (alternate 6).
 * PA12 - UART4_TX                  (alternate 6).
 * PA13 - SWDIO                     (alternate 0).
 * PA14 - SWCLK                     (alternate 0).
 * PA15 - I2S6_WS                   (alternate 7).
 */
#define VAL_GPIOA_MODER             (PIN_MODE_ANALOG(GPIOA_AGPIO0) |        \
                                     PIN_MODE_ALTERNATE(GPIOA_RMII_REF_CLK) |\
                                     PIN_MODE_ALTERNATE(GPIOA_RMII_MDIO) |  \
                                     PIN_MODE_ANALOG(GPIOA_AGPIO1) |        \
                                     PIN_MODE_ANALOG(GPIOA_AGPIO2) |        \
                                     PIN_MODE_ALTERNATE(GPIOA_SPI1_SCK) |   \
                                     PIN_MODE_ANALOG(GPIOA_AGPIO3) |        \
                                     PIN_MODE_ALTERNATE(GPIOA_RMII_CRS_DV) |\
                                     PIN_MODE_ALTERNATE(GPIOA_UART7_RX) |   \
                                     PIN_MODE_ALTERNATE(GPIOA_UART1_TX) |   \
                                     PIN_MODE_ALTERNATE(GPIOA_UART1_RX) |   \
                                     PIN_MODE_ALTERNATE(GPIOA_UART4_RX) |   \
                                     PIN_MODE_ALTERNATE(GPIOA_UART4_TX) |   \
                                     PIN_MODE_ALTERNATE(GPIOA_SWDIO) |      \
                                     PIN_MODE_ALTERNATE(GPIOA_SWCLK) |      \
                                     PIN_MODE_ALTERNATE(GPIOA_I2S6_WS))
#define VAL_GPIOA_OTYPER            (PIN_OTYPE_PUSHPULL(GPIOA_AGPIO0) |     \
                                     PIN_OTYPE_PUSHPULL(GPIOA_RMII_REF_CLK) |\
                                     PIN_OTYPE_PUSHPULL(GPIOA_RMII_MDIO) |  \
                                     PIN_OTYPE_PUSHPULL(GPIOA_AGPIO1) |     \
                                     PIN_OTYPE_PUSHPULL(GPIOA_AGPIO2) |     \
                                     PIN_OTYPE_PUSHPULL(GPIOA_SPI1_SCK) |   \
                                     PIN_OTYPE_PUSHPULL(GPIOA_AGPIO3) |     \
                                     PIN_OTYPE_PUSHPULL(GPIOA_RMII_CRS_DV) |\
                                     PIN_OTYPE_PUSHPULL(GPIOA_UART7_RX) |   \
                                     PIN_OTYPE_PUSHPULL(GPIOA_UART1_TX) |   \
                                     PIN_OTYPE_PUSHPULL(GPIOA_UART1_RX) |   \
                                     PIN_OTYPE_PUSHPULL(GPIOA_UART4_RX) |   \
                                     PIN_OTYPE_PUSHPULL(GPIOA_UART4_TX) |   \
                                     PIN_OTYPE_PUSHPULL(GPIOA_SWDIO) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOA_SWCLK) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOA_I2S6_WS))
#define VAL_GPIOA_OSPEEDR           (PIN_OSPEED_VERYLOW(GPIOA_AGPIO0) |     \
                                     PIN_OSPEED_HIGH(GPIOA_RMII_REF_CLK) |  \
                                     PIN_OSPEED_HIGH(GPIOA_RMII_MDIO) |     \
                                     PIN_OSPEED_VERYLOW(GPIOA_AGPIO1) |     \
                                     PIN_OSPEED_VERYLOW(GPIOA_AGPIO2) |     \
                                     PIN_OSPEED_VERYLOW(GPIOA_SPI1_SCK) |   \
                                     PIN_OSPEED_VERYLOW(GPIOA_AGPIO3) |     \
                                     PIN_OSPEED_HIGH(GPIOA_RMII_CRS_DV) |   \
                                     PIN_OSPEED_VERYLOW(GPIOA_UART7_RX) |   \
                                     PIN_OSPEED_VERYLOW(GPIOA_UART1_TX) |   \
                                     PIN_OSPEED_VERYLOW(GPIOA_UART1_RX) |   \
                                     PIN_OSPEED_VERYLOW(GPIOA_UART4_RX) |   \
                                     PIN_OSPEED_VERYLOW(GPIOA_UART4_TX) |   \
                                     PIN_OSPEED_HIGH(GPIOA_SWDIO) |         \
                                     PIN_OSPEED_HIGH(GPIOA_SWCLK) |         \
                                     PIN_OSPEED_VERYLOW(GPIOA_I2S6_WS))
#define VAL_GPIOA_PUPDR             (PIN_PUPDR_FLOATING(GPIOA_AGPIO0) |     \
                                     PIN_PUPDR_FLOATING(GPIOA_RMII_REF_CLK) |\
                                     PIN_PUPDR_PULLUP(GPIOA_RMII_MDIO) |    \
                                     PIN_PUPDR_FLOATING(GPIOA_AGPIO1) |     \
                                     PIN_PUPDR_FLOATING(GPIOA_AGPIO2) |     \
                                     PIN_PUPDR_FLOATING(GPIOA_SPI1_SCK) |   \
                                     PIN_PUPDR_FLOATING(GPIOA_AGPIO3) |     \
                                     PIN_PUPDR_PULLUP(GPIOA_RMII_CRS_DV) |  \
                                     PIN_PUPDR_FLOATING(GPIOA_UART7_RX) |   \
                                     PIN_PUPDR_FLOATING(GPIOA_UART1_TX) |   \
                                     PIN_PUPDR_FLOATING(GPIOA_UART1_RX) |   \
                                     PIN_PUPDR_FLOATING(GPIOA_UART4_RX) |   \
                                     PIN_PUPDR_FLOATING(GPIOA_UART4_TX) |   \
                                     PIN_PUPDR_FLOATING(GPIOA_SWDIO) |      \
                                     PIN_PUPDR_FLOATING(GPIOA_SWCLK) |      \
                                     PIN_PUPDR_FLOATING(GPIOA_I2S6_WS))
#define VAL_GPIOA_ODR               (PIN_ODR_LOW(GPIOA_AGPIO0) |            \
                                     PIN_ODR_HIGH(GPIOA_RMII_REF_CLK) |     \
                                     PIN_ODR_HIGH(GPIOA_RMII_MDIO) |        \
                                     PIN_ODR_LOW(GPIOA_AGPIO1) |            \
                                     PIN_ODR_LOW(GPIOA_AGPIO2) |            \
                                     PIN_ODR_LOW(GPIOA_SPI1_SCK) |          \
                                     PIN_ODR_LOW(GPIOA_AGPIO3) |            \
                                     PIN_ODR_HIGH(GPIOA_RMII_CRS_DV) |      \
                                     PIN_ODR_LOW(GPIOA_UART7_RX) |          \
                                     PIN_ODR_LOW(GPIOA_UART1_TX) |          \
                                     PIN_ODR_LOW(GPIOA_UART1_RX) |          \
                                     PIN_ODR_LOW(GPIOA_UART4_RX) |          \
                                     PIN_ODR_LOW(GPIOA_UART4_TX) |          \
                                     PIN_ODR_HIGH(GPIOA_SWDIO) |            \
                                     PIN_ODR_HIGH(GPIOA_SWCLK) |            \
                                     PIN_ODR_LOW(GPIOA_I2S6_WS))
#define VAL_GPIOA_AFRL              (PIN_AFIO_AF(GPIOA_AGPIO0, 0U) |        \
                                     PIN_AFIO_AF(GPIOA_RMII_REF_CLK, 11U) | \
                                     PIN_AFIO_AF(GPIOA_RMII_MDIO, 11U) |    \
                                     PIN_AFIO_AF(GPIOA_AGPIO1, 0U) |        \
                                     PIN_AFIO_AF(GPIOA_AGPIO2, 0U) |        \
                                     PIN_AFIO_AF(GPIOA_SPI1_SCK, 5U) |      \
                                     PIN_AFIO_AF(GPIOA_AGPIO3, 0U) |        \
                                     PIN_AFIO_AF(GPIOA_RMII_CRS_DV, 11U))
#define VAL_GPIOA_AFRH              (PIN_AFIO_AF(GPIOA_UART7_RX, 11U) |     \
                                     PIN_AFIO_AF(GPIOA_UART1_TX, 7U) |      \
                                     PIN_AFIO_AF(GPIOA_UART1_RX, 7U) |      \
                                     PIN_AFIO_AF(GPIOA_UART4_RX, 6U) |      \
                                     PIN_AFIO_AF(GPIOA_UART4_TX, 6U) |      \
                                     PIN_AFIO_AF(GPIOA_SWDIO, 0U) |         \
                                     PIN_AFIO_AF(GPIOA_SWCLK, 0U) |         \
                                     PIN_AFIO_AF(GPIOA_I2S6_WS, 7U))

/*
 * GPIOB setup:
 *
 * PB0  - AGPIO4                    (analog).
 * PB1  - AGPIO5                    (analog).
 * PB2  - SPI3_MOSI                 (alternate 7).
 * PB3  - SPI3_SCK                  (alternate 6).
 * PB4  - SPI3_MISO                 (alternate 6).
 * PB5  - I2S6_SDO                  (alternate 8).
 * PB6  - I2C1_SCL                  (alternate 4).
 * PB7  - I2C1_SDA                  (alternate 4).
 * PB8  - FDCAN1_RX                 (alternate 9).
 * PB9  - FDCAN1_TX                 (alternate 9).
 * PB10 - I2C2_SCL                  (alternate 4).
 * PB11 - I2C2_SDA                  (alternate 4).
 * PB12 - RMII_TXD0                 (alternate 11).
 * PB13 - OCTOSPI_IO2               (alternate 4).
 * PB14 - SPI2_MISO                 (alternate 5).
 * PB15 - SPI2_MOSI                 (alternate 5).
 */
#define VAL_GPIOB_MODER             (PIN_MODE_ANALOG(GPIOB_AGPIO4) |        \
                                     PIN_MODE_ANALOG(GPIOB_AGPIO5) |        \
                                     PIN_MODE_ALTERNATE(GPIOB_SPI3_MOSI) |  \
                                     PIN_MODE_ALTERNATE(GPIOB_SPI3_SCK) |   \
                                     PIN_MODE_ALTERNATE(GPIOB_SPI3_MISO) |  \
                                     PIN_MODE_ALTERNATE(GPIOB_I2S6_SDO) |   \
                                     PIN_MODE_ALTERNATE(GPIOB_I2C1_SCL) |   \
                                     PIN_MODE_ALTERNATE(GPIOB_I2C1_SDA) |   \
                                     PIN_MODE_ALTERNATE(GPIOB_FDCAN1_RX) |  \
                                     PIN_MODE_ALTERNATE(GPIOB_FDCAN1_TX) |  \
                                     PIN_MODE_ALTERNATE(GPIOB_I2C2_SCL) |   \
                                     PIN_MODE_ALTERNATE(GPIOB_I2C2_SDA) |   \
                                     PIN_MODE_ALTERNATE(GPIOB_RMII_TXD0) |  \
                                     PIN_MODE_ALTERNATE(GPIOB_OCTOSPI_IO2) |\
                                     PIN_MODE_ALTERNATE(GPIOB_SPI2_MISO) |  \
                                     PIN_MODE_ALTERNATE(GPIOB_SPI2_MOSI))
#define VAL_GPIOB_OTYPER            (PIN_OTYPE_PUSHPULL(GPIOB_AGPIO4) |     \
                                     PIN_OTYPE_PUSHPULL(GPIOB_AGPIO5) |     \
                                     PIN_OTYPE_PUSHPULL(GPIOB_SPI3_MOSI) |  \
                                     PIN_OTYPE_PUSHPULL(GPIOB_SPI3_SCK) |   \
                                     PIN_OTYPE_PUSHPULL(GPIOB_SPI3_MISO) |  \
                                     PIN_OTYPE_PUSHPULL(GPIOB_I2S6_SDO) |   \
                                     PIN_OTYPE_OPENDRAIN(GPIOB_I2C1_SCL) |  \
                                     PIN_OTYPE_OPENDRAIN(GPIOB_I2C1_SDA) |  \
                                     PIN_OTYPE_PUSHPULL(GPIOB_FDCAN1_RX) |  \
                                     PIN_OTYPE_PUSHPULL(GPIOB_FDCAN1_TX) |  \
                                     PIN_OTYPE_OPENDRAIN(GPIOB_I2C2_SCL) |  \
                                     PIN_OTYPE_OPENDRAIN(GPIOB_I2C2_SDA) |  \
                                     PIN_OTYPE_PUSHPULL(GPIOB_RMII_TXD0) |  \
                                     PIN_OTYPE_PUSHPULL(GPIOB_OCTOSPI_IO2) |\
                                     PIN_OTYPE_PUSHPULL(GPIOB_SPI2_MISO) |  \
                                     PIN_OTYPE_PUSHPULL(GPIOB_SPI2_MOSI))
#define VAL_GPIOB_OSPEEDR           (PIN_OSPEED_VERYLOW(GPIOB_AGPIO4) |     \
                                     PIN_OSPEED_VERYLOW(GPIOB_AGPIO5) |     \
                                     PIN_OSPEED_VERYLOW(GPIOB_SPI3_MOSI) |  \
                                     PIN_OSPEED_VERYLOW(GPIOB_SPI3_SCK) |   \
                                     PIN_OSPEED_VERYLOW(GPIOB_SPI3_MISO) |  \
                                     PIN_OSPEED_VERYLOW(GPIOB_I2S6_SDO) |   \
                                     PIN_OSPEED_VERYLOW(GPIOB_I2C1_SCL) |   \
                                     PIN_OSPEED_VERYLOW(GPIOB_I2C1_SDA) |   \
                                     PIN_OSPEED_VERYLOW(GPIOB_FDCAN1_RX) |  \
                                     PIN_OSPEED_VERYLOW(GPIOB_FDCAN1_TX) |  \
                                     PIN_OSPEED_VERYLOW(GPIOB_I2C2_SCL) |   \
                                     PIN_OSPEED_VERYLOW(GPIOB_I2C2_SDA) |   \
                                     PIN_OSPEED_HIGH(GPIOB_RMII_TXD0) |     \
                                     PIN_OSPEED_HIGH(GPIOB_OCTOSPI_IO2) |   \
                                     PIN_OSPEED_VERYLOW(GPIOB_SPI2_MISO) |  \
                                     PIN_OSPEED_VERYLOW(GPIOB_SPI2_MOSI))
#define VAL_GPIOB_PUPDR             (PIN_PUPDR_FLOATING(GPIOB_AGPIO4) |     \
                                     PIN_PUPDR_FLOATING(GPIOB_AGPIO5) |     \
                                     PIN_PUPDR_FLOATING(GPIOB_SPI3_MOSI) |  \
                                     PIN_PUPDR_FLOATING(GPIOB_SPI3_SCK) |   \
                                     PIN_PUPDR_FLOATING(GPIOB_SPI3_MISO) |  \
                                     PIN_PUPDR_FLOATING(GPIOB_I2S6_SDO) |   \
                                     PIN_PUPDR_FLOATING(GPIOB_I2C1_SCL) |   \
                                     PIN_PUPDR_FLOATING(GPIOB_I2C1_SDA) |   \
                                     PIN_PUPDR_FLOATING(GPIOB_FDCAN1_RX) |  \
                                     PIN_PUPDR_FLOATING(GPIOB_FDCAN1_TX) |  \
                                     PIN_PUPDR_FLOATING(GPIOB_I2C2_SCL) |   \
                                     PIN_PUPDR_FLOATING(GPIOB_I2C2_SDA) |   \
                                     PIN_PUPDR_FLOATING(GPIOB_RMII_TXD0) |  \
                                     PIN_PUPDR_FLOATING(GPIOB_OCTOSPI_IO2) |\
                                     PIN_PUPDR_FLOATING(GPIOB_SPI2_MISO) |  \
                                     PIN_PUPDR_FLOATING(GPIOB_SPI2_MOSI))
#define VAL_GPIOB_ODR               (PIN_ODR_LOW(GPIOB_AGPIO4) |            \
                                     PIN_ODR_LOW(GPIOB_AGPIO5) |            \
                                     PIN_ODR_LOW(GPIOB_SPI3_MOSI) |         \
                                     PIN_ODR_LOW(GPIOB_SPI3_SCK) |          \
                                     PIN_ODR_LOW(GPIOB_SPI3_MISO) |         \
                                     PIN_ODR_LOW(GPIOB_I2S6_SDO) |          \
                                     PIN_ODR_LOW(GPIOB_I2C1_SCL) |          \
                                     PIN_ODR_LOW(GPIOB_I2C1_SDA) |          \
                                     PIN_ODR_LOW(GPIOB_FDCAN1_RX) |         \
                                     PIN_ODR_LOW(GPIOB_FDCAN1_TX) |         \
                                     PIN_ODR_LOW(GPIOB_I2C2_SCL) |          \
                                     PIN_ODR_LOW(GPIOB_I2C2_SDA) |          \
                                     PIN_ODR_HIGH(GPIOB_RMII_TXD0) |        \
                                     PIN_ODR_HIGH(GPIOB_OCTOSPI_IO2) |      \
                                     PIN_ODR_LOW(GPIOB_SPI2_MISO) |         \
                                     PIN_ODR_LOW(GPIOB_SPI2_MOSI))
#define VAL_GPIOB_AFRL              (PIN_AFIO_AF(GPIOB_AGPIO4, 0U) |        \
                                     PIN_AFIO_AF(GPIOB_AGPIO5, 0U) |        \
                                     PIN_AFIO_AF(GPIOB_SPI3_MOSI, 7U) |     \
                                     PIN_AFIO_AF(GPIOB_SPI3_SCK, 6U) |      \
                                     PIN_AFIO_AF(GPIOB_SPI3_MISO, 6U) |     \
                                     PIN_AFIO_AF(GPIOB_I2S6_SDO, 8U) |      \
                                     PIN_AFIO_AF(GPIOB_I2C1_SCL, 4U) |      \
                                     PIN_AFIO_AF(GPIOB_I2C1_SDA, 4U))
#define VAL_GPIOB_AFRH              (PIN_AFIO_AF(GPIOB_FDCAN1_RX, 9U) |     \
                                     PIN_AFIO_AF(GPIOB_FDCAN1_TX, 9U) |     \
                                     PIN_AFIO_AF(GPIOB_I2C2_SCL, 4U) |      \
                                     PIN_AFIO_AF(GPIOB_I2C2_SDA, 4U) |      \
                                     PIN_AFIO_AF(GPIOB_RMII_TXD0, 11U) |    \
                                     PIN_AFIO_AF(GPIOB_OCTOSPI_IO2, 4U) |   \
                                     PIN_AFIO_AF(GPIOB_SPI2_MISO, 5U) |     \
                                     PIN_AFIO_AF(GPIOB_SPI2_MOSI, 5U))

/*
 * GPIOC setup:
 *
 * PC0  - PIN0                      (input pullup).
 * PC1  - RMII_MDC                  (alternate 11).
 * PC2  - PIN2                      (input pullup).
 * PC3  - PIN3                      (input pullup).
 * PC4  - RMII_RXD0                 (alternate 11).
 * PC5  - RMII_RXD1                 (alternate 11).
 * PC6  - UART6_TX                  (alternate 7).
 * PC7  - UART6_RX                  (alternate 7).
 * PC8  - PIN8                      (input pullup).
 * PC9  - PIN9                      (input pullup).
 * PC10 - PIN10                     (input pullup).
 * PC11 - PIN11                     (input pullup).
 * PC12 - PIN12                     (input pullup).
 * PC13 - HIGH_LEVEL_RUN_PG         (input floating).
 * PC14 - HIGH_LEVEL_GLOBAL_EN      (input floating).
 * PC15 - IMU_CS                    (output pushpull minimum).
 */
#define VAL_GPIOC_MODER             (PIN_MODE_INPUT(GPIOC_PIN0) |           \
                                     PIN_MODE_ALTERNATE(GPIOC_RMII_MDC) |   \
                                     PIN_MODE_INPUT(GPIOC_PIN2) |           \
                                     PIN_MODE_INPUT(GPIOC_PIN3) |           \
                                     PIN_MODE_ALTERNATE(GPIOC_RMII_RXD0) |  \
                                     PIN_MODE_ALTERNATE(GPIOC_RMII_RXD1) |  \
                                     PIN_MODE_ALTERNATE(GPIOC_UART6_TX) |   \
                                     PIN_MODE_ALTERNATE(GPIOC_UART6_RX) |   \
                                     PIN_MODE_INPUT(GPIOC_PIN8) |           \
                                     PIN_MODE_INPUT(GPIOC_PIN9) |           \
                                     PIN_MODE_INPUT(GPIOC_PIN10) |          \
                                     PIN_MODE_INPUT(GPIOC_PIN11) |          \
                                     PIN_MODE_INPUT(GPIOC_PIN12) |          \
                                     PIN_MODE_INPUT(GPIOC_HIGH_LEVEL_RUN_PG) |\
                                     PIN_MODE_INPUT(GPIOC_HIGH_LEVEL_GLOBAL_EN) |\
                                     PIN_MODE_OUTPUT(GPIOC_IMU_CS))
#define VAL_GPIOC_OTYPER            (PIN_OTYPE_PUSHPULL(GPIOC_PIN0) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOC_RMII_MDC) |   \
                                     PIN_OTYPE_PUSHPULL(GPIOC_PIN2) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOC_PIN3) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOC_RMII_RXD0) |  \
                                     PIN_OTYPE_PUSHPULL(GPIOC_RMII_RXD1) |  \
                                     PIN_OTYPE_PUSHPULL(GPIOC_UART6_TX) |   \
                                     PIN_OTYPE_PUSHPULL(GPIOC_UART6_RX) |   \
                                     PIN_OTYPE_PUSHPULL(GPIOC_PIN8) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOC_PIN9) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOC_PIN10) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOC_PIN11) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOC_PIN12) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOC_HIGH_LEVEL_RUN_PG) |\
                                     PIN_OTYPE_PUSHPULL(GPIOC_HIGH_LEVEL_GLOBAL_EN) |\
                                     PIN_OTYPE_PUSHPULL(GPIOC_IMU_CS))
#define VAL_GPIOC_OSPEEDR           (PIN_OSPEED_VERYLOW(GPIOC_PIN0) |       \
                                     PIN_OSPEED_HIGH(GPIOC_RMII_MDC) |      \
                                     PIN_OSPEED_VERYLOW(GPIOC_PIN2) |       \
                                     PIN_OSPEED_VERYLOW(GPIOC_PIN3) |       \
                                     PIN_OSPEED_HIGH(GPIOC_RMII_RXD0) |     \
                                     PIN_OSPEED_HIGH(GPIOC_RMII_RXD1) |     \
                                     PIN_OSPEED_VERYLOW(GPIOC_UART6_TX) |   \
                                     PIN_OSPEED_VERYLOW(GPIOC_UART6_RX) |   \
                                     PIN_OSPEED_VERYLOW(GPIOC_PIN8) |       \
                                     PIN_OSPEED_VERYLOW(GPIOC_PIN9) |       \
                                     PIN_OSPEED_VERYLOW(GPIOC_PIN10) |      \
                                     PIN_OSPEED_VERYLOW(GPIOC_PIN11) |      \
                                     PIN_OSPEED_VERYLOW(GPIOC_PIN12) |      \
                                     PIN_OSPEED_VERYLOW(GPIOC_HIGH_LEVEL_RUN_PG) |\
                                     PIN_OSPEED_VERYLOW(GPIOC_HIGH_LEVEL_GLOBAL_EN) |\
                                     PIN_OSPEED_VERYLOW(GPIOC_IMU_CS))
#define VAL_GPIOC_PUPDR             (PIN_PUPDR_PULLUP(GPIOC_PIN0) |         \
                                     PIN_PUPDR_FLOATING(GPIOC_RMII_MDC) |   \
                                     PIN_PUPDR_PULLUP(GPIOC_PIN2) |         \
                                     PIN_PUPDR_PULLUP(GPIOC_PIN3) |         \
                                     PIN_PUPDR_FLOATING(GPIOC_RMII_RXD0) |  \
                                     PIN_PUPDR_FLOATING(GPIOC_RMII_RXD1) |  \
                                     PIN_PUPDR_FLOATING(GPIOC_UART6_TX) |   \
                                     PIN_PUPDR_FLOATING(GPIOC_UART6_RX) |   \
                                     PIN_PUPDR_PULLUP(GPIOC_PIN8) |         \
                                     PIN_PUPDR_PULLUP(GPIOC_PIN9) |         \
                                     PIN_PUPDR_PULLUP(GPIOC_PIN10) |        \
                                     PIN_PUPDR_PULLUP(GPIOC_PIN11) |        \
                                     PIN_PUPDR_PULLUP(GPIOC_PIN12) |        \
                                     PIN_PUPDR_FLOATING(GPIOC_HIGH_LEVEL_RUN_PG) |\
                                     PIN_PUPDR_FLOATING(GPIOC_HIGH_LEVEL_GLOBAL_EN) |\
                                     PIN_PUPDR_FLOATING(GPIOC_IMU_CS))
#define VAL_GPIOC_ODR               (PIN_ODR_HIGH(GPIOC_PIN0) |             \
                                     PIN_ODR_HIGH(GPIOC_RMII_MDC) |         \
                                     PIN_ODR_HIGH(GPIOC_PIN2) |             \
                                     PIN_ODR_HIGH(GPIOC_PIN3) |             \
                                     PIN_ODR_HIGH(GPIOC_RMII_RXD0) |        \
                                     PIN_ODR_HIGH(GPIOC_RMII_RXD1) |        \
                                     PIN_ODR_LOW(GPIOC_UART6_TX) |          \
                                     PIN_ODR_LOW(GPIOC_UART6_RX) |          \
                                     PIN_ODR_HIGH(GPIOC_PIN8) |             \
                                     PIN_ODR_HIGH(GPIOC_PIN9) |             \
                                     PIN_ODR_HIGH(GPIOC_PIN10) |            \
                                     PIN_ODR_HIGH(GPIOC_PIN11) |            \
                                     PIN_ODR_HIGH(GPIOC_PIN12) |            \
                                     PIN_ODR_HIGH(GPIOC_HIGH_LEVEL_RUN_PG) |\
                                     PIN_ODR_HIGH(GPIOC_HIGH_LEVEL_GLOBAL_EN) |\
                                     PIN_ODR_HIGH(GPIOC_IMU_CS))
#define VAL_GPIOC_AFRL              (PIN_AFIO_AF(GPIOC_PIN0, 0U) |          \
                                     PIN_AFIO_AF(GPIOC_RMII_MDC, 11U) |     \
                                     PIN_AFIO_AF(GPIOC_PIN2, 0U) |          \
                                     PIN_AFIO_AF(GPIOC_PIN3, 0U) |          \
                                     PIN_AFIO_AF(GPIOC_RMII_RXD0, 11U) |    \
                                     PIN_AFIO_AF(GPIOC_RMII_RXD1, 11U) |    \
                                     PIN_AFIO_AF(GPIOC_UART6_TX, 7U) |      \
                                     PIN_AFIO_AF(GPIOC_UART6_RX, 7U))
#define VAL_GPIOC_AFRH              (PIN_AFIO_AF(GPIOC_PIN8, 0U) |          \
                                     PIN_AFIO_AF(GPIOC_PIN9, 0U) |          \
                                     PIN_AFIO_AF(GPIOC_PIN10, 0U) |         \
                                     PIN_AFIO_AF(GPIOC_PIN11, 0U) |         \
                                     PIN_AFIO_AF(GPIOC_PIN12, 0U) |         \
                                     PIN_AFIO_AF(GPIOC_HIGH_LEVEL_RUN_PG, 0U) |\
                                     PIN_AFIO_AF(GPIOC_HIGH_LEVEL_GLOBAL_EN, 0U) |\
                                     PIN_AFIO_AF(GPIOC_IMU_CS, 0U))

/*
 * GPIOD setup:
 *
 * PD0  - PIN0                      (input pullup).
 * PD1  - PIN1                      (input pullup).
 * PD2  - PIN2                      (input pullup).
 * PD3  - SPI2_SCK                  (alternate 5).
 * PD4  - GPIO0                     (input floating).
 * PD5  - UART2_TX                  (alternate 7).
 * PD6  - UART2_RX                  (alternate 7).
 * PD7  - SPI1_MOSI                 (alternate 5).
 * PD8  - GPIO8                     (input floating).
 * PD9  - GPIO15                    (input floating).
 * PD10 - GPIO10                    (input floating).
 * PD11 - GPIO1                     (input floating).
 * PD12 - I2C4_SCL                  (alternate 4).
 * PD13 - I2C4_SDA                  (alternate 4).
 * PD14 - PIN14                     (input pullup).
 * PD15 - IMU_INTERRUPT             (input pullup).
 */
#define VAL_GPIOD_MODER             (PIN_MODE_INPUT(GPIOD_PIN0) |           \
                                     PIN_MODE_INPUT(GPIOD_PIN1) |           \
                                     PIN_MODE_INPUT(GPIOD_PIN2) |           \
                                     PIN_MODE_ALTERNATE(GPIOD_SPI2_SCK) |   \
                                     PIN_MODE_INPUT(GPIOD_GPIO0) |          \
                                     PIN_MODE_ALTERNATE(GPIOD_UART2_TX) |   \
                                     PIN_MODE_ALTERNATE(GPIOD_UART2_RX) |   \
                                     PIN_MODE_ALTERNATE(GPIOD_SPI1_MOSI) |  \
                                     PIN_MODE_INPUT(GPIOD_GPIO8) |          \
                                     PIN_MODE_INPUT(GPIOD_GPIO15) |         \
                                     PIN_MODE_INPUT(GPIOD_GPIO10) |         \
                                     PIN_MODE_INPUT(GPIOD_GPIO1) |          \
                                     PIN_MODE_ALTERNATE(GPIOD_I2C4_SCL) |   \
                                     PIN_MODE_ALTERNATE(GPIOD_I2C4_SDA) |   \
                                     PIN_MODE_INPUT(GPIOD_PIN14) |          \
                                     PIN_MODE_INPUT(GPIOD_IMU_INTERRUPT))
#define VAL_GPIOD_OTYPER            (PIN_OTYPE_PUSHPULL(GPIOD_PIN0) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOD_PIN1) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOD_PIN2) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOD_SPI2_SCK) |   \
                                     PIN_OTYPE_PUSHPULL(GPIOD_GPIO0) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOD_UART2_TX) |   \
                                     PIN_OTYPE_PUSHPULL(GPIOD_UART2_RX) |   \
                                     PIN_OTYPE_PUSHPULL(GPIOD_SPI1_MOSI) |  \
                                     PIN_OTYPE_PUSHPULL(GPIOD_GPIO8) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOD_GPIO15) |     \
                                     PIN_OTYPE_PUSHPULL(GPIOD_GPIO10) |     \
                                     PIN_OTYPE_PUSHPULL(GPIOD_GPIO1) |      \
                                     PIN_OTYPE_OPENDRAIN(GPIOD_I2C4_SCL) |  \
                                     PIN_OTYPE_OPENDRAIN(GPIOD_I2C4_SDA) |  \
                                     PIN_OTYPE_PUSHPULL(GPIOD_PIN14) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOD_IMU_INTERRUPT))
#define VAL_GPIOD_OSPEEDR           (PIN_OSPEED_VERYLOW(GPIOD_PIN0) |       \
                                     PIN_OSPEED_VERYLOW(GPIOD_PIN1) |       \
                                     PIN_OSPEED_VERYLOW(GPIOD_PIN2) |       \
                                     PIN_OSPEED_VERYLOW(GPIOD_SPI2_SCK) |   \
                                     PIN_OSPEED_VERYLOW(GPIOD_GPIO0) |      \
                                     PIN_OSPEED_VERYLOW(GPIOD_UART2_TX) |   \
                                     PIN_OSPEED_VERYLOW(GPIOD_UART2_RX) |   \
                                     PIN_OSPEED_VERYLOW(GPIOD_SPI1_MOSI) |  \
                                     PIN_OSPEED_VERYLOW(GPIOD_GPIO8) |      \
                                     PIN_OSPEED_VERYLOW(GPIOD_GPIO15) |     \
                                     PIN_OSPEED_VERYLOW(GPIOD_GPIO10) |     \
                                     PIN_OSPEED_VERYLOW(GPIOD_GPIO1) |      \
                                     PIN_OSPEED_VERYLOW(GPIOD_I2C4_SCL) |   \
                                     PIN_OSPEED_VERYLOW(GPIOD_I2C4_SDA) |   \
                                     PIN_OSPEED_VERYLOW(GPIOD_PIN14) |      \
                                     PIN_OSPEED_VERYLOW(GPIOD_IMU_INTERRUPT))
#define VAL_GPIOD_PUPDR             (PIN_PUPDR_PULLUP(GPIOD_PIN0) |         \
                                     PIN_PUPDR_PULLUP(GPIOD_PIN1) |         \
                                     PIN_PUPDR_PULLUP(GPIOD_PIN2) |         \
                                     PIN_PUPDR_FLOATING(GPIOD_SPI2_SCK) |   \
                                     PIN_PUPDR_FLOATING(GPIOD_GPIO0) |      \
                                     PIN_PUPDR_FLOATING(GPIOD_UART2_TX) |   \
                                     PIN_PUPDR_FLOATING(GPIOD_UART2_RX) |   \
                                     PIN_PUPDR_FLOATING(GPIOD_SPI1_MOSI) |  \
                                     PIN_PUPDR_FLOATING(GPIOD_GPIO8) |      \
                                     PIN_PUPDR_FLOATING(GPIOD_GPIO15) |     \
                                     PIN_PUPDR_FLOATING(GPIOD_GPIO10) |     \
                                     PIN_PUPDR_FLOATING(GPIOD_GPIO1) |      \
                                     PIN_PUPDR_FLOATING(GPIOD_I2C4_SCL) |   \
                                     PIN_PUPDR_FLOATING(GPIOD_I2C4_SDA) |   \
                                     PIN_PUPDR_PULLUP(GPIOD_PIN14) |        \
                                     PIN_PUPDR_PULLUP(GPIOD_IMU_INTERRUPT))
#define VAL_GPIOD_ODR               (PIN_ODR_HIGH(GPIOD_PIN0) |             \
                                     PIN_ODR_HIGH(GPIOD_PIN1) |             \
                                     PIN_ODR_HIGH(GPIOD_PIN2) |             \
                                     PIN_ODR_LOW(GPIOD_SPI2_SCK) |          \
                                     PIN_ODR_LOW(GPIOD_GPIO0) |             \
                                     PIN_ODR_LOW(GPIOD_UART2_TX) |          \
                                     PIN_ODR_LOW(GPIOD_UART2_RX) |          \
                                     PIN_ODR_LOW(GPIOD_SPI1_MOSI) |         \
                                     PIN_ODR_LOW(GPIOD_GPIO8) |             \
                                     PIN_ODR_LOW(GPIOD_GPIO15) |            \
                                     PIN_ODR_LOW(GPIOD_GPIO10) |            \
                                     PIN_ODR_LOW(GPIOD_GPIO1) |             \
                                     PIN_ODR_LOW(GPIOD_I2C4_SCL) |          \
                                     PIN_ODR_LOW(GPIOD_I2C4_SDA) |          \
                                     PIN_ODR_HIGH(GPIOD_PIN14) |            \
                                     PIN_ODR_HIGH(GPIOD_IMU_INTERRUPT))
#define VAL_GPIOD_AFRL              (PIN_AFIO_AF(GPIOD_PIN0, 0U) |          \
                                     PIN_AFIO_AF(GPIOD_PIN1, 0U) |          \
                                     PIN_AFIO_AF(GPIOD_PIN2, 0U) |          \
                                     PIN_AFIO_AF(GPIOD_SPI2_SCK, 5U) |      \
                                     PIN_AFIO_AF(GPIOD_GPIO0, 0U) |         \
                                     PIN_AFIO_AF(GPIOD_UART2_TX, 7U) |      \
                                     PIN_AFIO_AF(GPIOD_UART2_RX, 7U) |      \
                                     PIN_AFIO_AF(GPIOD_SPI1_MOSI, 5U))
#define VAL_GPIOD_AFRH              (PIN_AFIO_AF(GPIOD_GPIO8, 0U) |         \
                                     PIN_AFIO_AF(GPIOD_GPIO15, 0U) |        \
                                     PIN_AFIO_AF(GPIOD_GPIO10, 0U) |        \
                                     PIN_AFIO_AF(GPIOD_GPIO1, 0U) |         \
                                     PIN_AFIO_AF(GPIOD_I2C4_SCL, 4U) |      \
                                     PIN_AFIO_AF(GPIOD_I2C4_SDA, 4U) |      \
                                     PIN_AFIO_AF(GPIOD_PIN14, 0U) |         \
                                     PIN_AFIO_AF(GPIOD_IMU_INTERRUPT, 0U))

/*
 * GPIOE setup:
 *
 * PE0  - UART8_RX                  (alternate 8).
 * PE1  - UART8_TX                  (alternate 8).
 * PE2  - UART10_RX                 (alternate 4).
 * PE3  - UART10_TX                 (alternate 11).
 * PE4  - GPIO2                     (input floating).
 * PE5  - GPIO3                     (input floating).
 * PE6  - GPIO4                     (input floating).
 * PE7  - GPIO21                    (input floating).
 * PE8  - GPIO22                    (input floating).
 * PE9  - GPIO23                    (input floating).
 * PE10 - GPIO9                     (input floating).
 * PE11 - GPIO20                    (input floating).
 * PE12 - GPIO19                    (input floating).
 * PE13 - GPIO18                    (input floating).
 * PE14 - GPIO17                    (input floating).
 * PE15 - GPIO16                    (input floating).
 */
#define VAL_GPIOE_MODER             (PIN_MODE_ALTERNATE(GPIOE_UART8_RX) |   \
                                     PIN_MODE_ALTERNATE(GPIOE_UART8_TX) |   \
                                     PIN_MODE_ALTERNATE(GPIOE_UART10_RX) |  \
                                     PIN_MODE_ALTERNATE(GPIOE_UART10_TX) |  \
                                     PIN_MODE_INPUT(GPIOE_GPIO2) |          \
                                     PIN_MODE_INPUT(GPIOE_GPIO3) |          \
                                     PIN_MODE_INPUT(GPIOE_GPIO4) |          \
                                     PIN_MODE_INPUT(GPIOE_GPIO21) |         \
                                     PIN_MODE_INPUT(GPIOE_GPIO22) |         \
                                     PIN_MODE_INPUT(GPIOE_GPIO23) |         \
                                     PIN_MODE_INPUT(GPIOE_GPIO9) |          \
                                     PIN_MODE_INPUT(GPIOE_GPIO20) |         \
                                     PIN_MODE_INPUT(GPIOE_GPIO19) |         \
                                     PIN_MODE_INPUT(GPIOE_GPIO18) |         \
                                     PIN_MODE_INPUT(GPIOE_GPIO17) |         \
                                     PIN_MODE_INPUT(GPIOE_GPIO16))
#define VAL_GPIOE_OTYPER            (PIN_OTYPE_PUSHPULL(GPIOE_UART8_RX) |   \
                                     PIN_OTYPE_PUSHPULL(GPIOE_UART8_TX) |   \
                                     PIN_OTYPE_PUSHPULL(GPIOE_UART10_RX) |  \
                                     PIN_OTYPE_PUSHPULL(GPIOE_UART10_TX) |  \
                                     PIN_OTYPE_PUSHPULL(GPIOE_GPIO2) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOE_GPIO3) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOE_GPIO4) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOE_GPIO21) |     \
                                     PIN_OTYPE_PUSHPULL(GPIOE_GPIO22) |     \
                                     PIN_OTYPE_PUSHPULL(GPIOE_GPIO23) |     \
                                     PIN_OTYPE_PUSHPULL(GPIOE_GPIO9) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOE_GPIO20) |     \
                                     PIN_OTYPE_PUSHPULL(GPIOE_GPIO19) |     \
                                     PIN_OTYPE_PUSHPULL(GPIOE_GPIO18) |     \
                                     PIN_OTYPE_PUSHPULL(GPIOE_GPIO17) |     \
                                     PIN_OTYPE_PUSHPULL(GPIOE_GPIO16))
#define VAL_GPIOE_OSPEEDR           (PIN_OSPEED_VERYLOW(GPIOE_UART8_RX) |   \
                                     PIN_OSPEED_VERYLOW(GPIOE_UART8_TX) |   \
                                     PIN_OSPEED_VERYLOW(GPIOE_UART10_RX) |  \
                                     PIN_OSPEED_VERYLOW(GPIOE_UART10_TX) |  \
                                     PIN_OSPEED_VERYLOW(GPIOE_GPIO2) |      \
                                     PIN_OSPEED_VERYLOW(GPIOE_GPIO3) |      \
                                     PIN_OSPEED_VERYLOW(GPIOE_GPIO4) |      \
                                     PIN_OSPEED_VERYLOW(GPIOE_GPIO21) |     \
                                     PIN_OSPEED_VERYLOW(GPIOE_GPIO22) |     \
                                     PIN_OSPEED_VERYLOW(GPIOE_GPIO23) |     \
                                     PIN_OSPEED_VERYLOW(GPIOE_GPIO9) |      \
                                     PIN_OSPEED_VERYLOW(GPIOE_GPIO20) |     \
                                     PIN_OSPEED_VERYLOW(GPIOE_GPIO19) |     \
                                     PIN_OSPEED_VERYLOW(GPIOE_GPIO18) |     \
                                     PIN_OSPEED_VERYLOW(GPIOE_GPIO17) |     \
                                     PIN_OSPEED_VERYLOW(GPIOE_GPIO16))
#define VAL_GPIOE_PUPDR             (PIN_PUPDR_FLOATING(GPIOE_UART8_RX) |   \
                                     PIN_PUPDR_FLOATING(GPIOE_UART8_TX) |   \
                                     PIN_PUPDR_FLOATING(GPIOE_UART10_RX) |  \
                                     PIN_PUPDR_FLOATING(GPIOE_UART10_TX) |  \
                                     PIN_PUPDR_FLOATING(GPIOE_GPIO2) |      \
                                     PIN_PUPDR_FLOATING(GPIOE_GPIO3) |      \
                                     PIN_PUPDR_FLOATING(GPIOE_GPIO4) |      \
                                     PIN_PUPDR_FLOATING(GPIOE_GPIO21) |     \
                                     PIN_PUPDR_FLOATING(GPIOE_GPIO22) |     \
                                     PIN_PUPDR_FLOATING(GPIOE_GPIO23) |     \
                                     PIN_PUPDR_FLOATING(GPIOE_GPIO9) |      \
                                     PIN_PUPDR_FLOATING(GPIOE_GPIO20) |     \
                                     PIN_PUPDR_FLOATING(GPIOE_GPIO19) |     \
                                     PIN_PUPDR_FLOATING(GPIOE_GPIO18) |     \
                                     PIN_PUPDR_FLOATING(GPIOE_GPIO17) |     \
                                     PIN_PUPDR_FLOATING(GPIOE_GPIO16))
#define VAL_GPIOE_ODR               (PIN_ODR_LOW(GPIOE_UART8_RX) |          \
                                     PIN_ODR_LOW(GPIOE_UART8_TX) |          \
                                     PIN_ODR_LOW(GPIOE_UART10_RX) |         \
                                     PIN_ODR_LOW(GPIOE_UART10_TX) |         \
                                     PIN_ODR_LOW(GPIOE_GPIO2) |             \
                                     PIN_ODR_LOW(GPIOE_GPIO3) |             \
                                     PIN_ODR_LOW(GPIOE_GPIO4) |             \
                                     PIN_ODR_LOW(GPIOE_GPIO21) |            \
                                     PIN_ODR_LOW(GPIOE_GPIO22) |            \
                                     PIN_ODR_LOW(GPIOE_GPIO23) |            \
                                     PIN_ODR_LOW(GPIOE_GPIO9) |             \
                                     PIN_ODR_LOW(GPIOE_GPIO20) |            \
                                     PIN_ODR_LOW(GPIOE_GPIO19) |            \
                                     PIN_ODR_LOW(GPIOE_GPIO18) |            \
                                     PIN_ODR_LOW(GPIOE_GPIO17) |            \
                                     PIN_ODR_LOW(GPIOE_GPIO16))
#define VAL_GPIOE_AFRL              (PIN_AFIO_AF(GPIOE_UART8_RX, 8U) |      \
                                     PIN_AFIO_AF(GPIOE_UART8_TX, 8U) |      \
                                     PIN_AFIO_AF(GPIOE_UART10_RX, 4U) |     \
                                     PIN_AFIO_AF(GPIOE_UART10_TX, 11U) |    \
                                     PIN_AFIO_AF(GPIOE_GPIO2, 0U) |         \
                                     PIN_AFIO_AF(GPIOE_GPIO3, 0U) |         \
                                     PIN_AFIO_AF(GPIOE_GPIO4, 0U) |         \
                                     PIN_AFIO_AF(GPIOE_GPIO21, 0U))
#define VAL_GPIOE_AFRH              (PIN_AFIO_AF(GPIOE_GPIO22, 0U) |        \
                                     PIN_AFIO_AF(GPIOE_GPIO23, 0U) |        \
                                     PIN_AFIO_AF(GPIOE_GPIO9, 0U) |         \
                                     PIN_AFIO_AF(GPIOE_GPIO20, 0U) |        \
                                     PIN_AFIO_AF(GPIOE_GPIO19, 0U) |        \
                                     PIN_AFIO_AF(GPIOE_GPIO18, 0U) |        \
                                     PIN_AFIO_AF(GPIOE_GPIO17, 0U) |        \
                                     PIN_AFIO_AF(GPIOE_GPIO16, 0U))

/*
 * GPIOF setup:
 *
 * PF0  - HEARTBEAT_LED_RED         (output pushpull minimum).
 * PF1  - HEARTBEAT_LED_GREEN       (output pushpull minimum).
 * PF2  - HEARTBEAT_LED_BLUE        (output pushpull minimum).
 * PF3  - RESET_PHY                 (output opendrain minimum).
 * PF4  - PIN4                      (input pullup).
 * PF5  - PIN5                      (input pullup).
 * PF6  - OCTOSPI_IO3               (alternate 10).
 * PF7  - UART7_TX                  (alternate 7).
 * PF8  - OCTOSPI_IO0               (alternate 10).
 * PF9  - OCTOSPI_IO1               (alternate 10).
 * PF10 - OCTOSPI_CLK               (alternate 9).
 * PF11 - STATUS_LED_RED            (output pushpull minimum).
 * PF12 - STATUS_LED_GREEN STATUS_LED(output pushpull minimum).
 * PF13 - STATUS_LED_BLUE           (output pushpull minimum).
 * PF14 - PIN14                     (input pullup).
 * PF15 - PIN15                     (input pullup).
 */
#define VAL_GPIOF_MODER             (PIN_MODE_OUTPUT(GPIOF_HEARTBEAT_LED_RED) |\
                                     PIN_MODE_OUTPUT(GPIOF_HEARTBEAT_LED_GREEN) |\
                                     PIN_MODE_OUTPUT(GPIOF_HEARTBEAT_LED_BLUE) |\
                                     PIN_MODE_OUTPUT(GPIOF_RESET_PHY) |     \
                                     PIN_MODE_INPUT(GPIOF_PIN4) |           \
                                     PIN_MODE_INPUT(GPIOF_PIN5) |           \
                                     PIN_MODE_ALTERNATE(GPIOF_OCTOSPI_IO3) |\
                                     PIN_MODE_ALTERNATE(GPIOF_UART7_TX) |   \
                                     PIN_MODE_ALTERNATE(GPIOF_OCTOSPI_IO0) |\
                                     PIN_MODE_ALTERNATE(GPIOF_OCTOSPI_IO1) |\
                                     PIN_MODE_ALTERNATE(GPIOF_OCTOSPI_CLK) |\
                                     PIN_MODE_OUTPUT(GPIOF_STATUS_LED_RED) |\
                                     PIN_MODE_OUTPUT(GPIOF_STATUS_LED_GREEN) |\
                                     PIN_MODE_OUTPUT(GPIOF_STATUS_LED_BLUE) |\
                                     PIN_MODE_INPUT(GPIOF_PIN14) |          \
                                     PIN_MODE_INPUT(GPIOF_PIN15))
#define VAL_GPIOF_OTYPER            (PIN_OTYPE_PUSHPULL(GPIOF_HEARTBEAT_LED_RED) |\
                                     PIN_OTYPE_PUSHPULL(GPIOF_HEARTBEAT_LED_GREEN) |\
                                     PIN_OTYPE_PUSHPULL(GPIOF_HEARTBEAT_LED_BLUE) |\
                                     PIN_OTYPE_OPENDRAIN(GPIOF_RESET_PHY) | \
                                     PIN_OTYPE_PUSHPULL(GPIOF_PIN4) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOF_PIN5) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOF_OCTOSPI_IO3) |\
                                     PIN_OTYPE_PUSHPULL(GPIOF_UART7_TX) |   \
                                     PIN_OTYPE_PUSHPULL(GPIOF_OCTOSPI_IO0) |\
                                     PIN_OTYPE_PUSHPULL(GPIOF_OCTOSPI_IO1) |\
                                     PIN_OTYPE_PUSHPULL(GPIOF_OCTOSPI_CLK) |\
                                     PIN_OTYPE_PUSHPULL(GPIOF_STATUS_LED_RED) |\
                                     PIN_OTYPE_PUSHPULL(GPIOF_STATUS_LED_GREEN) |\
                                     PIN_OTYPE_PUSHPULL(GPIOF_STATUS_LED_BLUE) |\
                                     PIN_OTYPE_PUSHPULL(GPIOF_PIN14) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOF_PIN15))
#define VAL_GPIOF_OSPEEDR           (PIN_OSPEED_VERYLOW(GPIOF_HEARTBEAT_LED_RED) |\
                                     PIN_OSPEED_VERYLOW(GPIOF_HEARTBEAT_LED_GREEN) |\
                                     PIN_OSPEED_VERYLOW(GPIOF_HEARTBEAT_LED_BLUE) |\
                                     PIN_OSPEED_VERYLOW(GPIOF_RESET_PHY) |  \
                                     PIN_OSPEED_VERYLOW(GPIOF_PIN4) |       \
                                     PIN_OSPEED_VERYLOW(GPIOF_PIN5) |       \
                                     PIN_OSPEED_HIGH(GPIOF_OCTOSPI_IO3) |   \
                                     PIN_OSPEED_VERYLOW(GPIOF_UART7_TX) |   \
                                     PIN_OSPEED_HIGH(GPIOF_OCTOSPI_IO0) |   \
                                     PIN_OSPEED_HIGH(GPIOF_OCTOSPI_IO1) |   \
                                     PIN_OSPEED_HIGH(GPIOF_OCTOSPI_CLK) |   \
                                     PIN_OSPEED_VERYLOW(GPIOF_STATUS_LED_RED) |\
                                     PIN_OSPEED_VERYLOW(GPIOF_STATUS_LED_GREEN) |\
                                     PIN_OSPEED_VERYLOW(GPIOF_STATUS_LED_BLUE) |\
                                     PIN_OSPEED_VERYLOW(GPIOF_PIN14) |      \
                                     PIN_OSPEED_VERYLOW(GPIOF_PIN15))
#define VAL_GPIOF_PUPDR             (PIN_PUPDR_PULLUP(GPIOF_HEARTBEAT_LED_RED) |\
                                     PIN_PUPDR_PULLUP(GPIOF_HEARTBEAT_LED_GREEN) |\
                                     PIN_PUPDR_PULLUP(GPIOF_HEARTBEAT_LED_BLUE) |\
                                     PIN_PUPDR_FLOATING(GPIOF_RESET_PHY) |  \
                                     PIN_PUPDR_PULLUP(GPIOF_PIN4) |         \
                                     PIN_PUPDR_PULLUP(GPIOF_PIN5) |         \
                                     PIN_PUPDR_FLOATING(GPIOF_OCTOSPI_IO3) |\
                                     PIN_PUPDR_FLOATING(GPIOF_UART7_TX) |   \
                                     PIN_PUPDR_FLOATING(GPIOF_OCTOSPI_IO0) |\
                                     PIN_PUPDR_FLOATING(GPIOF_OCTOSPI_IO1) |\
                                     PIN_PUPDR_FLOATING(GPIOF_OCTOSPI_CLK) |\
                                     PIN_PUPDR_PULLUP(GPIOF_STATUS_LED_RED) |\
                                     PIN_PUPDR_PULLUP(GPIOF_STATUS_LED_GREEN) |\
                                     PIN_PUPDR_PULLUP(GPIOF_STATUS_LED_BLUE) |\
                                     PIN_PUPDR_PULLUP(GPIOF_PIN14) |        \
                                     PIN_PUPDR_PULLUP(GPIOF_PIN15))
#define VAL_GPIOF_ODR               (PIN_ODR_HIGH(GPIOF_HEARTBEAT_LED_RED) |\
                                     PIN_ODR_HIGH(GPIOF_HEARTBEAT_LED_GREEN) |\
                                     PIN_ODR_HIGH(GPIOF_HEARTBEAT_LED_BLUE) |\
                                     PIN_ODR_HIGH(GPIOF_RESET_PHY) |        \
                                     PIN_ODR_HIGH(GPIOF_PIN4) |             \
                                     PIN_ODR_HIGH(GPIOF_PIN5) |             \
                                     PIN_ODR_HIGH(GPIOF_OCTOSPI_IO3) |      \
                                     PIN_ODR_LOW(GPIOF_UART7_TX) |          \
                                     PIN_ODR_HIGH(GPIOF_OCTOSPI_IO0) |      \
                                     PIN_ODR_HIGH(GPIOF_OCTOSPI_IO1) |      \
                                     PIN_ODR_HIGH(GPIOF_OCTOSPI_CLK) |      \
                                     PIN_ODR_HIGH(GPIOF_STATUS_LED_RED) |   \
                                     PIN_ODR_HIGH(GPIOF_STATUS_LED_GREEN) | \
                                     PIN_ODR_HIGH(GPIOF_STATUS_LED_BLUE) |  \
                                     PIN_ODR_HIGH(GPIOF_PIN14) |            \
                                     PIN_ODR_HIGH(GPIOF_PIN15))
#define VAL_GPIOF_AFRL              (PIN_AFIO_AF(GPIOF_HEARTBEAT_LED_RED, 0U) |\
                                     PIN_AFIO_AF(GPIOF_HEARTBEAT_LED_GREEN, 0U) |\
                                     PIN_AFIO_AF(GPIOF_HEARTBEAT_LED_BLUE, 0U) |\
                                     PIN_AFIO_AF(GPIOF_RESET_PHY, 0U) |     \
                                     PIN_AFIO_AF(GPIOF_PIN4, 0U) |          \
                                     PIN_AFIO_AF(GPIOF_PIN5, 0U) |          \
                                     PIN_AFIO_AF(GPIOF_OCTOSPI_IO3, 10U) |  \
                                     PIN_AFIO_AF(GPIOF_UART7_TX, 7U))
#define VAL_GPIOF_AFRH              (PIN_AFIO_AF(GPIOF_OCTOSPI_IO0, 10U) |  \
                                     PIN_AFIO_AF(GPIOF_OCTOSPI_IO1, 10U) |  \
                                     PIN_AFIO_AF(GPIOF_OCTOSPI_CLK, 9U) |   \
                                     PIN_AFIO_AF(GPIOF_STATUS_LED_RED, 0U) |\
                                     PIN_AFIO_AF(GPIOF_STATUS_LED_GREEN, 0U) |\
                                     PIN_AFIO_AF(GPIOF_STATUS_LED_BLUE, 0U) |\
                                     PIN_AFIO_AF(GPIOF_PIN14, 0U) |         \
                                     PIN_AFIO_AF(GPIOF_PIN15, 0U))

/*
 * GPIOG setup:
 *
 * PG0  - PIN0                      (input pullup).
 * PG1  - PIN1                      (input pullup).
 * PG2  - GPIO14                    (input floating).
 * PG3  - GPIO5                     (input floating).
 * PG4  - GPIO13                    (input floating).
 * PG5  - GPIO12                    (input floating).
 * PG6  - OCTOSPI_NCS               (alternate 10).
 * PG7  - GPIO6                     (input floating).
 * PG8  - GPIO11                    (input floating).
 * PG9  - SPI1_MISO                 (alternate 11).
 * PG10 - GPIO7                     (input floating).
 * PG11 - RMII_TX_EN                (alternate 11).
 * PG12 - I2S6_SDI                  (alternate 5).
 * PG13 - I2S6_CK                   (alternate 5).
 * PG14 - RMII_TXD1                 (alternate 11).
 * PG15 - PIN15                     (input pullup).
 */
#define VAL_GPIOG_MODER             (PIN_MODE_INPUT(GPIOG_PIN0) |           \
                                     PIN_MODE_INPUT(GPIOG_PIN1) |           \
                                     PIN_MODE_INPUT(GPIOG_GPIO14) |         \
                                     PIN_MODE_INPUT(GPIOG_GPIO5) |          \
                                     PIN_MODE_INPUT(GPIOG_GPIO13) |         \
                                     PIN_MODE_INPUT(GPIOG_GPIO12) |         \
                                     PIN_MODE_ALTERNATE(GPIOG_OCTOSPI_NCS) |\
                                     PIN_MODE_INPUT(GPIOG_GPIO6) |          \
                                     PIN_MODE_INPUT(GPIOG_GPIO11) |         \
                                     PIN_MODE_ALTERNATE(GPIOG_SPI1_MISO) |  \
                                     PIN_MODE_INPUT(GPIOG_GPIO7) |          \
                                     PIN_MODE_ALTERNATE(GPIOG_RMII_TX_EN) | \
                                     PIN_MODE_ALTERNATE(GPIOG_I2S6_SDI) |   \
                                     PIN_MODE_ALTERNATE(GPIOG_I2S6_CK) |    \
                                     PIN_MODE_ALTERNATE(GPIOG_RMII_TXD1) |  \
                                     PIN_MODE_INPUT(GPIOG_PIN15))
#define VAL_GPIOG_OTYPER            (PIN_OTYPE_PUSHPULL(GPIOG_PIN0) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOG_PIN1) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOG_GPIO14) |     \
                                     PIN_OTYPE_PUSHPULL(GPIOG_GPIO5) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOG_GPIO13) |     \
                                     PIN_OTYPE_PUSHPULL(GPIOG_GPIO12) |     \
                                     PIN_OTYPE_PUSHPULL(GPIOG_OCTOSPI_NCS) |\
                                     PIN_OTYPE_PUSHPULL(GPIOG_GPIO6) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOG_GPIO11) |     \
                                     PIN_OTYPE_PUSHPULL(GPIOG_SPI1_MISO) |  \
                                     PIN_OTYPE_PUSHPULL(GPIOG_GPIO7) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOG_RMII_TX_EN) | \
                                     PIN_OTYPE_PUSHPULL(GPIOG_I2S6_SDI) |   \
                                     PIN_OTYPE_PUSHPULL(GPIOG_I2S6_CK) |    \
                                     PIN_OTYPE_PUSHPULL(GPIOG_RMII_TXD1) |  \
                                     PIN_OTYPE_PUSHPULL(GPIOG_PIN15))
#define VAL_GPIOG_OSPEEDR           (PIN_OSPEED_VERYLOW(GPIOG_PIN0) |       \
                                     PIN_OSPEED_VERYLOW(GPIOG_PIN1) |       \
                                     PIN_OSPEED_VERYLOW(GPIOG_GPIO14) |     \
                                     PIN_OSPEED_VERYLOW(GPIOG_GPIO5) |      \
                                     PIN_OSPEED_VERYLOW(GPIOG_GPIO13) |     \
                                     PIN_OSPEED_VERYLOW(GPIOG_GPIO12) |     \
                                     PIN_OSPEED_VERYLOW(GPIOG_OCTOSPI_NCS) |\
                                     PIN_OSPEED_VERYLOW(GPIOG_GPIO6) |      \
                                     PIN_OSPEED_VERYLOW(GPIOG_GPIO11) |     \
                                     PIN_OSPEED_VERYLOW(GPIOG_SPI1_MISO) |  \
                                     PIN_OSPEED_VERYLOW(GPIOG_GPIO7) |      \
                                     PIN_OSPEED_HIGH(GPIOG_RMII_TX_EN) |    \
                                     PIN_OSPEED_VERYLOW(GPIOG_I2S6_SDI) |   \
                                     PIN_OSPEED_VERYLOW(GPIOG_I2S6_CK) |    \
                                     PIN_OSPEED_HIGH(GPIOG_RMII_TXD1) |     \
                                     PIN_OSPEED_VERYLOW(GPIOG_PIN15))
#define VAL_GPIOG_PUPDR             (PIN_PUPDR_PULLUP(GPIOG_PIN0) |         \
                                     PIN_PUPDR_PULLUP(GPIOG_PIN1) |         \
                                     PIN_PUPDR_FLOATING(GPIOG_GPIO14) |     \
                                     PIN_PUPDR_FLOATING(GPIOG_GPIO5) |      \
                                     PIN_PUPDR_FLOATING(GPIOG_GPIO13) |     \
                                     PIN_PUPDR_FLOATING(GPIOG_GPIO12) |     \
                                     PIN_PUPDR_FLOATING(GPIOG_OCTOSPI_NCS) |\
                                     PIN_PUPDR_FLOATING(GPIOG_GPIO6) |      \
                                     PIN_PUPDR_FLOATING(GPIOG_GPIO11) |     \
                                     PIN_PUPDR_FLOATING(GPIOG_SPI1_MISO) |  \
                                     PIN_PUPDR_FLOATING(GPIOG_GPIO7) |      \
                                     PIN_PUPDR_FLOATING(GPIOG_RMII_TX_EN) | \
                                     PIN_PUPDR_FLOATING(GPIOG_I2S6_SDI) |   \
                                     PIN_PUPDR_FLOATING(GPIOG_I2S6_CK) |    \
                                     PIN_PUPDR_PULLUP(GPIOG_RMII_TXD1) |    \
                                     PIN_PUPDR_PULLUP(GPIOG_PIN15))
#define VAL_GPIOG_ODR               (PIN_ODR_HIGH(GPIOG_PIN0) |             \
                                     PIN_ODR_HIGH(GPIOG_PIN1) |             \
                                     PIN_ODR_LOW(GPIOG_GPIO14) |            \
                                     PIN_ODR_LOW(GPIOG_GPIO5) |             \
                                     PIN_ODR_LOW(GPIOG_GPIO13) |            \
                                     PIN_ODR_LOW(GPIOG_GPIO12) |            \
                                     PIN_ODR_LOW(GPIOG_OCTOSPI_NCS) |       \
                                     PIN_ODR_LOW(GPIOG_GPIO6) |             \
                                     PIN_ODR_LOW(GPIOG_GPIO11) |            \
                                     PIN_ODR_LOW(GPIOG_SPI1_MISO) |         \
                                     PIN_ODR_LOW(GPIOG_GPIO7) |             \
                                     PIN_ODR_HIGH(GPIOG_RMII_TX_EN) |       \
                                     PIN_ODR_LOW(GPIOG_I2S6_SDI) |          \
                                     PIN_ODR_LOW(GPIOG_I2S6_CK) |           \
                                     PIN_ODR_HIGH(GPIOG_RMII_TXD1) |        \
                                     PIN_ODR_HIGH(GPIOG_PIN15))
#define VAL_GPIOG_AFRL              (PIN_AFIO_AF(GPIOG_PIN0, 0U) |          \
                                     PIN_AFIO_AF(GPIOG_PIN1, 0U) |          \
                                     PIN_AFIO_AF(GPIOG_GPIO14, 0U) |        \
                                     PIN_AFIO_AF(GPIOG_GPIO5, 0U) |         \
                                     PIN_AFIO_AF(GPIOG_GPIO13, 0U) |        \
                                     PIN_AFIO_AF(GPIOG_GPIO12, 0U) |        \
                                     PIN_AFIO_AF(GPIOG_OCTOSPI_NCS, 10U) |  \
                                     PIN_AFIO_AF(GPIOG_GPIO6, 0U))
#define VAL_GPIOG_AFRH              (PIN_AFIO_AF(GPIOG_GPIO11, 0U) |        \
                                     PIN_AFIO_AF(GPIOG_SPI1_MISO, 11U) |    \
                                     PIN_AFIO_AF(GPIOG_GPIO7, 0U) |         \
                                     PIN_AFIO_AF(GPIOG_RMII_TX_EN, 11U) |   \
                                     PIN_AFIO_AF(GPIOG_I2S6_SDI, 5U) |      \
                                     PIN_AFIO_AF(GPIOG_I2S6_CK, 5U) |       \
                                     PIN_AFIO_AF(GPIOG_RMII_TXD1, 11U) |    \
                                     PIN_AFIO_AF(GPIOG_PIN15, 0U))

/*
 * GPIOH setup:
 *
 * PH0  - OSC_IN                    (input floating).
 * PH1  - OSC_OUT                   (input floating).
 * PH2  - PIN2                      (input pullup).
 * PH3  - PIN3                      (input pullup).
 * PH4  - PIN4                      (input pullup).
 * PH5  - PIN5                      (input pullup).
 * PH6  - PIN6                      (input pullup).
 * PH7  - PIN7                      (input pullup).
 * PH8  - PIN8                      (input pullup).
 * PH9  - PIN9                      (input pullup).
 * PH10 - PIN10                     (input pullup).
 * PH11 - PIN11                     (input pullup).
 * PH12 - PIN12                     (input pullup).
 * PH13 - PIN13                     (input pullup).
 * PH14 - PIN14                     (input pullup).
 * PH15 - PIN15                     (input pullup).
 */
#define VAL_GPIOH_MODER             (PIN_MODE_INPUT(GPIOH_OSC_IN) |         \
                                     PIN_MODE_INPUT(GPIOH_OSC_OUT) |        \
                                     PIN_MODE_INPUT(GPIOH_PIN2) |           \
                                     PIN_MODE_INPUT(GPIOH_PIN3) |           \
                                     PIN_MODE_INPUT(GPIOH_PIN4) |           \
                                     PIN_MODE_INPUT(GPIOH_PIN5) |           \
                                     PIN_MODE_INPUT(GPIOH_PIN6) |           \
                                     PIN_MODE_INPUT(GPIOH_PIN7) |           \
                                     PIN_MODE_INPUT(GPIOH_PIN8) |           \
                                     PIN_MODE_INPUT(GPIOH_PIN9) |           \
                                     PIN_MODE_INPUT(GPIOH_PIN10) |          \
                                     PIN_MODE_INPUT(GPIOH_PIN11) |          \
                                     PIN_MODE_INPUT(GPIOH_PIN12) |          \
                                     PIN_MODE_INPUT(GPIOH_PIN13) |          \
                                     PIN_MODE_INPUT(GPIOH_PIN14) |          \
                                     PIN_MODE_INPUT(GPIOH_PIN15))
#define VAL_GPIOH_OTYPER            (PIN_OTYPE_PUSHPULL(GPIOH_OSC_IN) |     \
                                     PIN_OTYPE_PUSHPULL(GPIOH_OSC_OUT) |    \
                                     PIN_OTYPE_PUSHPULL(GPIOH_PIN2) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOH_PIN3) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOH_PIN4) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOH_PIN5) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOH_PIN6) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOH_PIN7) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOH_PIN8) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOH_PIN9) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOH_PIN10) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOH_PIN11) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOH_PIN12) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOH_PIN13) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOH_PIN14) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOH_PIN15))
#define VAL_GPIOH_OSPEEDR           (PIN_OSPEED_HIGH(GPIOH_OSC_IN) |        \
                                     PIN_OSPEED_HIGH(GPIOH_OSC_OUT) |       \
                                     PIN_OSPEED_VERYLOW(GPIOH_PIN2) |       \
                                     PIN_OSPEED_VERYLOW(GPIOH_PIN3) |       \
                                     PIN_OSPEED_VERYLOW(GPIOH_PIN4) |       \
                                     PIN_OSPEED_VERYLOW(GPIOH_PIN5) |       \
                                     PIN_OSPEED_VERYLOW(GPIOH_PIN6) |       \
                                     PIN_OSPEED_VERYLOW(GPIOH_PIN7) |       \
                                     PIN_OSPEED_VERYLOW(GPIOH_PIN8) |       \
                                     PIN_OSPEED_VERYLOW(GPIOH_PIN9) |       \
                                     PIN_OSPEED_VERYLOW(GPIOH_PIN10) |      \
                                     PIN_OSPEED_VERYLOW(GPIOH_PIN11) |      \
                                     PIN_OSPEED_VERYLOW(GPIOH_PIN12) |      \
                                     PIN_OSPEED_VERYLOW(GPIOH_PIN13) |      \
                                     PIN_OSPEED_VERYLOW(GPIOH_PIN14) |      \
                                     PIN_OSPEED_VERYLOW(GPIOH_PIN15))
#define VAL_GPIOH_PUPDR             (PIN_PUPDR_FLOATING(GPIOH_OSC_IN) |     \
                                     PIN_PUPDR_FLOATING(GPIOH_OSC_OUT) |    \
                                     PIN_PUPDR_PULLUP(GPIOH_PIN2) |         \
                                     PIN_PUPDR_PULLUP(GPIOH_PIN3) |         \
                                     PIN_PUPDR_PULLUP(GPIOH_PIN4) |         \
                                     PIN_PUPDR_PULLUP(GPIOH_PIN5) |         \
                                     PIN_PUPDR_PULLUP(GPIOH_PIN6) |         \
                                     PIN_PUPDR_PULLUP(GPIOH_PIN7) |         \
                                     PIN_PUPDR_PULLUP(GPIOH_PIN8) |         \
                                     PIN_PUPDR_PULLUP(GPIOH_PIN9) |         \
                                     PIN_PUPDR_PULLUP(GPIOH_PIN10) |        \
                                     PIN_PUPDR_PULLUP(GPIOH_PIN11) |        \
                                     PIN_PUPDR_PULLUP(GPIOH_PIN12) |        \
                                     PIN_PUPDR_PULLUP(GPIOH_PIN13) |        \
                                     PIN_PUPDR_PULLUP(GPIOH_PIN14) |        \
                                     PIN_PUPDR_PULLUP(GPIOH_PIN15))
#define VAL_GPIOH_ODR               (PIN_ODR_HIGH(GPIOH_OSC_IN) |           \
                                     PIN_ODR_HIGH(GPIOH_OSC_OUT) |          \
                                     PIN_ODR_HIGH(GPIOH_PIN2) |             \
                                     PIN_ODR_HIGH(GPIOH_PIN3) |             \
                                     PIN_ODR_HIGH(GPIOH_PIN4) |             \
                                     PIN_ODR_HIGH(GPIOH_PIN5) |             \
                                     PIN_ODR_HIGH(GPIOH_PIN6) |             \
                                     PIN_ODR_HIGH(GPIOH_PIN7) |             \
                                     PIN_ODR_HIGH(GPIOH_PIN8) |             \
                                     PIN_ODR_HIGH(GPIOH_PIN9) |             \
                                     PIN_ODR_HIGH(GPIOH_PIN10) |            \
                                     PIN_ODR_HIGH(GPIOH_PIN11) |            \
                                     PIN_ODR_HIGH(GPIOH_PIN12) |            \
                                     PIN_ODR_HIGH(GPIOH_PIN13) |            \
                                     PIN_ODR_HIGH(GPIOH_PIN14) |            \
                                     PIN_ODR_HIGH(GPIOH_PIN15))
#define VAL_GPIOH_AFRL              (PIN_AFIO_AF(GPIOH_OSC_IN, 0U) |        \
                                     PIN_AFIO_AF(GPIOH_OSC_OUT, 0U) |       \
                                     PIN_AFIO_AF(GPIOH_PIN2, 0U) |          \
                                     PIN_AFIO_AF(GPIOH_PIN3, 0U) |          \
                                     PIN_AFIO_AF(GPIOH_PIN4, 0U) |          \
                                     PIN_AFIO_AF(GPIOH_PIN5, 0U) |          \
                                     PIN_AFIO_AF(GPIOH_PIN6, 0U) |          \
                                     PIN_AFIO_AF(GPIOH_PIN7, 0U))
#define VAL_GPIOH_AFRH              (PIN_AFIO_AF(GPIOH_PIN8, 0U) |          \
                                     PIN_AFIO_AF(GPIOH_PIN9, 0U) |          \
                                     PIN_AFIO_AF(GPIOH_PIN10, 0U) |         \
                                     PIN_AFIO_AF(GPIOH_PIN11, 0U) |         \
                                     PIN_AFIO_AF(GPIOH_PIN12, 0U) |         \
                                     PIN_AFIO_AF(GPIOH_PIN13, 0U) |         \
                                     PIN_AFIO_AF(GPIOH_PIN14, 0U) |         \
                                     PIN_AFIO_AF(GPIOH_PIN15, 0U))

/*
 * GPIOI setup:
 *
 * PI0  - PIN0                      (input pullup).
 * PI1  - PIN1                      (input pullup).
 * PI2  - PIN2                      (input pullup).
 * PI3  - PIN3                      (input pullup).
 * PI4  - PIN4                      (input pullup).
 * PI5  - PIN5                      (input pullup).
 * PI6  - PIN6                      (input pullup).
 * PI7  - PIN7                      (input pullup).
 * PI8  - PIN8                      (input pullup).
 * PI9  - PIN9                      (input pullup).
 * PI10 - PIN10                     (input pullup).
 * PI11 - PIN11                     (input pullup).
 * PI12 - PIN12                     (input pullup).
 * PI13 - PIN13                     (input pullup).
 * PI14 - PIN14                     (input pullup).
 * PI15 - PIN15                     (input pullup).
 */
#define VAL_GPIOI_MODER             (PIN_MODE_INPUT(GPIOI_PIN0) |           \
                                     PIN_MODE_INPUT(GPIOI_PIN1) |           \
                                     PIN_MODE_INPUT(GPIOI_PIN2) |           \
                                     PIN_MODE_INPUT(GPIOI_PIN3) |           \
                                     PIN_MODE_INPUT(GPIOI_PIN4) |           \
                                     PIN_MODE_INPUT(GPIOI_PIN5) |           \
                                     PIN_MODE_INPUT(GPIOI_PIN6) |           \
                                     PIN_MODE_INPUT(GPIOI_PIN7) |           \
                                     PIN_MODE_INPUT(GPIOI_PIN8) |           \
                                     PIN_MODE_INPUT(GPIOI_PIN9) |           \
                                     PIN_MODE_INPUT(GPIOI_PIN10) |          \
                                     PIN_MODE_INPUT(GPIOI_PIN11) |          \
                                     PIN_MODE_INPUT(GPIOI_PIN12) |          \
                                     PIN_MODE_INPUT(GPIOI_PIN13) |          \
                                     PIN_MODE_INPUT(GPIOI_PIN14) |          \
                                     PIN_MODE_INPUT(GPIOI_PIN15))
#define VAL_GPIOI_OTYPER            (PIN_OTYPE_PUSHPULL(GPIOI_PIN0) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOI_PIN1) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOI_PIN2) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOI_PIN3) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOI_PIN4) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOI_PIN5) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOI_PIN6) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOI_PIN7) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOI_PIN8) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOI_PIN9) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOI_PIN10) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOI_PIN11) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOI_PIN12) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOI_PIN13) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOI_PIN14) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOI_PIN15))
#define VAL_GPIOI_OSPEEDR           (PIN_OSPEED_VERYLOW(GPIOI_PIN0) |       \
                                     PIN_OSPEED_VERYLOW(GPIOI_PIN1) |       \
                                     PIN_OSPEED_VERYLOW(GPIOI_PIN2) |       \
                                     PIN_OSPEED_VERYLOW(GPIOI_PIN3) |       \
                                     PIN_OSPEED_VERYLOW(GPIOI_PIN4) |       \
                                     PIN_OSPEED_VERYLOW(GPIOI_PIN5) |       \
                                     PIN_OSPEED_VERYLOW(GPIOI_PIN6) |       \
                                     PIN_OSPEED_VERYLOW(GPIOI_PIN7) |       \
                                     PIN_OSPEED_VERYLOW(GPIOI_PIN8) |       \
                                     PIN_OSPEED_VERYLOW(GPIOI_PIN9) |       \
                                     PIN_OSPEED_VERYLOW(GPIOI_PIN10) |      \
                                     PIN_OSPEED_VERYLOW(GPIOI_PIN11) |      \
                                     PIN_OSPEED_VERYLOW(GPIOI_PIN12) |      \
                                     PIN_OSPEED_VERYLOW(GPIOI_PIN13) |      \
                                     PIN_OSPEED_VERYLOW(GPIOI_PIN14) |      \
                                     PIN_OSPEED_VERYLOW(GPIOI_PIN15))
#define VAL_GPIOI_PUPDR             (PIN_PUPDR_PULLUP(GPIOI_PIN0) |         \
                                     PIN_PUPDR_PULLUP(GPIOI_PIN1) |         \
                                     PIN_PUPDR_PULLUP(GPIOI_PIN2) |         \
                                     PIN_PUPDR_PULLUP(GPIOI_PIN3) |         \
                                     PIN_PUPDR_PULLUP(GPIOI_PIN4) |         \
                                     PIN_PUPDR_PULLUP(GPIOI_PIN5) |         \
                                     PIN_PUPDR_PULLUP(GPIOI_PIN6) |         \
                                     PIN_PUPDR_PULLUP(GPIOI_PIN7) |         \
                                     PIN_PUPDR_PULLUP(GPIOI_PIN8) |         \
                                     PIN_PUPDR_PULLUP(GPIOI_PIN9) |         \
                                     PIN_PUPDR_PULLUP(GPIOI_PIN10) |        \
                                     PIN_PUPDR_PULLUP(GPIOI_PIN11) |        \
                                     PIN_PUPDR_PULLUP(GPIOI_PIN12) |        \
                                     PIN_PUPDR_PULLUP(GPIOI_PIN13) |        \
                                     PIN_PUPDR_PULLUP(GPIOI_PIN14) |        \
                                     PIN_PUPDR_PULLUP(GPIOI_PIN15))
#define VAL_GPIOI_ODR               (PIN_ODR_HIGH(GPIOI_PIN0) |             \
                                     PIN_ODR_HIGH(GPIOI_PIN1) |             \
                                     PIN_ODR_HIGH(GPIOI_PIN2) |             \
                                     PIN_ODR_HIGH(GPIOI_PIN3) |             \
                                     PIN_ODR_HIGH(GPIOI_PIN4) |             \
                                     PIN_ODR_HIGH(GPIOI_PIN5) |             \
                                     PIN_ODR_HIGH(GPIOI_PIN6) |             \
                                     PIN_ODR_HIGH(GPIOI_PIN7) |             \
                                     PIN_ODR_HIGH(GPIOI_PIN8) |             \
                                     PIN_ODR_HIGH(GPIOI_PIN9) |             \
                                     PIN_ODR_HIGH(GPIOI_PIN10) |            \
                                     PIN_ODR_HIGH(GPIOI_PIN11) |            \
                                     PIN_ODR_HIGH(GPIOI_PIN12) |            \
                                     PIN_ODR_HIGH(GPIOI_PIN13) |            \
                                     PIN_ODR_HIGH(GPIOI_PIN14) |            \
                                     PIN_ODR_HIGH(GPIOI_PIN15))
#define VAL_GPIOI_AFRL              (PIN_AFIO_AF(GPIOI_PIN0, 0U) |          \
                                     PIN_AFIO_AF(GPIOI_PIN1, 0U) |          \
                                     PIN_AFIO_AF(GPIOI_PIN2, 0U) |          \
                                     PIN_AFIO_AF(GPIOI_PIN3, 0U) |          \
                                     PIN_AFIO_AF(GPIOI_PIN4, 0U) |          \
                                     PIN_AFIO_AF(GPIOI_PIN5, 0U) |          \
                                     PIN_AFIO_AF(GPIOI_PIN6, 0U) |          \
                                     PIN_AFIO_AF(GPIOI_PIN7, 0U))
#define VAL_GPIOI_AFRH              (PIN_AFIO_AF(GPIOI_PIN8, 0U) |          \
                                     PIN_AFIO_AF(GPIOI_PIN9, 0U) |          \
                                     PIN_AFIO_AF(GPIOI_PIN10, 0U) |         \
                                     PIN_AFIO_AF(GPIOI_PIN11, 0U) |         \
                                     PIN_AFIO_AF(GPIOI_PIN12, 0U) |         \
                                     PIN_AFIO_AF(GPIOI_PIN13, 0U) |         \
                                     PIN_AFIO_AF(GPIOI_PIN14, 0U) |         \
                                     PIN_AFIO_AF(GPIOI_PIN15, 0U))

/*
 * GPIOJ setup:
 *
 * PJ0  - PIN0                      (input pullup).
 * PJ1  - PIN1                      (input pullup).
 * PJ2  - PIN2                      (input pullup).
 * PJ3  - PIN3                      (input pullup).
 * PJ4  - PIN4                      (input pullup).
 * PJ5  - PIN5                      (input pullup).
 * PJ6  - PIN6                      (input pullup).
 * PJ7  - PIN7                      (input pullup).
 * PJ8  - PIN8                      (input pullup).
 * PJ9  - PIN9                      (input pullup).
 * PJ10 - PIN10                     (input pullup).
 * PJ11 - PIN11                     (input pullup).
 * PJ12 - PIN12                     (input pullup).
 * PJ13 - PIN13                     (input pullup).
 * PJ14 - PIN14                     (input pullup).
 * PJ15 - PIN15                     (input pullup).
 */
#define VAL_GPIOJ_MODER             (PIN_MODE_INPUT(GPIOJ_PIN0) |           \
                                     PIN_MODE_INPUT(GPIOJ_PIN1) |           \
                                     PIN_MODE_INPUT(GPIOJ_PIN2) |           \
                                     PIN_MODE_INPUT(GPIOJ_PIN3) |           \
                                     PIN_MODE_INPUT(GPIOJ_PIN4) |           \
                                     PIN_MODE_INPUT(GPIOJ_PIN5) |           \
                                     PIN_MODE_INPUT(GPIOJ_PIN6) |           \
                                     PIN_MODE_INPUT(GPIOJ_PIN7) |           \
                                     PIN_MODE_INPUT(GPIOJ_PIN8) |           \
                                     PIN_MODE_INPUT(GPIOJ_PIN9) |           \
                                     PIN_MODE_INPUT(GPIOJ_PIN10) |          \
                                     PIN_MODE_INPUT(GPIOJ_PIN11) |          \
                                     PIN_MODE_INPUT(GPIOJ_PIN12) |          \
                                     PIN_MODE_INPUT(GPIOJ_PIN13) |          \
                                     PIN_MODE_INPUT(GPIOJ_PIN14) |          \
                                     PIN_MODE_INPUT(GPIOJ_PIN15))
#define VAL_GPIOJ_OTYPER            (PIN_OTYPE_PUSHPULL(GPIOJ_PIN0) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOJ_PIN1) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOJ_PIN2) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOJ_PIN3) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOJ_PIN4) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOJ_PIN5) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOJ_PIN6) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOJ_PIN7) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOJ_PIN8) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOJ_PIN9) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOJ_PIN10) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOJ_PIN11) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOJ_PIN12) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOJ_PIN13) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOJ_PIN14) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOJ_PIN15))
#define VAL_GPIOJ_OSPEEDR           (PIN_OSPEED_VERYLOW(GPIOJ_PIN0) |       \
                                     PIN_OSPEED_VERYLOW(GPIOJ_PIN1) |       \
                                     PIN_OSPEED_VERYLOW(GPIOJ_PIN2) |       \
                                     PIN_OSPEED_VERYLOW(GPIOJ_PIN3) |       \
                                     PIN_OSPEED_VERYLOW(GPIOJ_PIN4) |       \
                                     PIN_OSPEED_VERYLOW(GPIOJ_PIN5) |       \
                                     PIN_OSPEED_VERYLOW(GPIOJ_PIN6) |       \
                                     PIN_OSPEED_VERYLOW(GPIOJ_PIN7) |       \
                                     PIN_OSPEED_VERYLOW(GPIOJ_PIN8) |       \
                                     PIN_OSPEED_VERYLOW(GPIOJ_PIN9) |       \
                                     PIN_OSPEED_VERYLOW(GPIOJ_PIN10) |      \
                                     PIN_OSPEED_VERYLOW(GPIOJ_PIN11) |      \
                                     PIN_OSPEED_VERYLOW(GPIOJ_PIN12) |      \
                                     PIN_OSPEED_VERYLOW(GPIOJ_PIN13) |      \
                                     PIN_OSPEED_VERYLOW(GPIOJ_PIN14) |      \
                                     PIN_OSPEED_VERYLOW(GPIOJ_PIN15))
#define VAL_GPIOJ_PUPDR             (PIN_PUPDR_PULLUP(GPIOJ_PIN0) |         \
                                     PIN_PUPDR_PULLUP(GPIOJ_PIN1) |         \
                                     PIN_PUPDR_PULLUP(GPIOJ_PIN2) |         \
                                     PIN_PUPDR_PULLUP(GPIOJ_PIN3) |         \
                                     PIN_PUPDR_PULLUP(GPIOJ_PIN4) |         \
                                     PIN_PUPDR_PULLUP(GPIOJ_PIN5) |         \
                                     PIN_PUPDR_PULLUP(GPIOJ_PIN6) |         \
                                     PIN_PUPDR_PULLUP(GPIOJ_PIN7) |         \
                                     PIN_PUPDR_PULLUP(GPIOJ_PIN8) |         \
                                     PIN_PUPDR_PULLUP(GPIOJ_PIN9) |         \
                                     PIN_PUPDR_PULLUP(GPIOJ_PIN10) |        \
                                     PIN_PUPDR_PULLUP(GPIOJ_PIN11) |        \
                                     PIN_PUPDR_PULLUP(GPIOJ_PIN12) |        \
                                     PIN_PUPDR_PULLUP(GPIOJ_PIN13) |        \
                                     PIN_PUPDR_PULLUP(GPIOJ_PIN14) |        \
                                     PIN_PUPDR_PULLUP(GPIOJ_PIN15))
#define VAL_GPIOJ_ODR               (PIN_ODR_HIGH(GPIOJ_PIN0) |             \
                                     PIN_ODR_HIGH(GPIOJ_PIN1) |             \
                                     PIN_ODR_HIGH(GPIOJ_PIN2) |             \
                                     PIN_ODR_HIGH(GPIOJ_PIN3) |             \
                                     PIN_ODR_HIGH(GPIOJ_PIN4) |             \
                                     PIN_ODR_HIGH(GPIOJ_PIN5) |             \
                                     PIN_ODR_HIGH(GPIOJ_PIN6) |             \
                                     PIN_ODR_HIGH(GPIOJ_PIN7) |             \
                                     PIN_ODR_HIGH(GPIOJ_PIN8) |             \
                                     PIN_ODR_HIGH(GPIOJ_PIN9) |             \
                                     PIN_ODR_HIGH(GPIOJ_PIN10) |            \
                                     PIN_ODR_HIGH(GPIOJ_PIN11) |            \
                                     PIN_ODR_HIGH(GPIOJ_PIN12) |            \
                                     PIN_ODR_HIGH(GPIOJ_PIN13) |            \
                                     PIN_ODR_HIGH(GPIOJ_PIN14) |            \
                                     PIN_ODR_HIGH(GPIOJ_PIN15))
#define VAL_GPIOJ_AFRL              (PIN_AFIO_AF(GPIOJ_PIN0, 0U) |          \
                                     PIN_AFIO_AF(GPIOJ_PIN1, 0U) |          \
                                     PIN_AFIO_AF(GPIOJ_PIN2, 0U) |          \
                                     PIN_AFIO_AF(GPIOJ_PIN3, 0U) |          \
                                     PIN_AFIO_AF(GPIOJ_PIN4, 0U) |          \
                                     PIN_AFIO_AF(GPIOJ_PIN5, 0U) |          \
                                     PIN_AFIO_AF(GPIOJ_PIN6, 0U) |          \
                                     PIN_AFIO_AF(GPIOJ_PIN7, 0U))
#define VAL_GPIOJ_AFRH              (PIN_AFIO_AF(GPIOJ_PIN8, 0U) |          \
                                     PIN_AFIO_AF(GPIOJ_PIN9, 0U) |          \
                                     PIN_AFIO_AF(GPIOJ_PIN10, 0U) |         \
                                     PIN_AFIO_AF(GPIOJ_PIN11, 0U) |         \
                                     PIN_AFIO_AF(GPIOJ_PIN12, 0U) |         \
                                     PIN_AFIO_AF(GPIOJ_PIN13, 0U) |         \
                                     PIN_AFIO_AF(GPIOJ_PIN14, 0U) |         \
                                     PIN_AFIO_AF(GPIOJ_PIN15, 0U))

/*
 * GPIOK setup:
 *
 * PK0  - PIN0                      (input pullup).
 * PK1  - PIN1                      (input pullup).
 * PK2  - PIN2                      (input pullup).
 * PK3  - PIN3                      (input pullup).
 * PK4  - PIN4                      (input pullup).
 * PK5  - PIN5                      (input pullup).
 * PK6  - PIN6                      (input pullup).
 * PK7  - PIN7                      (input pullup).
 * PK8  - PIN8                      (input pullup).
 * PK9  - PIN9                      (input pullup).
 * PK10 - PIN10                     (input pullup).
 * PK11 - PIN11                     (input pullup).
 * PK12 - PIN12                     (input pullup).
 * PK13 - PIN13                     (input pullup).
 * PK14 - PIN14                     (input pullup).
 * PK15 - PIN15                     (input pullup).
 */
#define VAL_GPIOK_MODER             (PIN_MODE_INPUT(GPIOK_PIN0) |           \
                                     PIN_MODE_INPUT(GPIOK_PIN1) |           \
                                     PIN_MODE_INPUT(GPIOK_PIN2) |           \
                                     PIN_MODE_INPUT(GPIOK_PIN3) |           \
                                     PIN_MODE_INPUT(GPIOK_PIN4) |           \
                                     PIN_MODE_INPUT(GPIOK_PIN5) |           \
                                     PIN_MODE_INPUT(GPIOK_PIN6) |           \
                                     PIN_MODE_INPUT(GPIOK_PIN7) |           \
                                     PIN_MODE_INPUT(GPIOK_PIN8) |           \
                                     PIN_MODE_INPUT(GPIOK_PIN9) |           \
                                     PIN_MODE_INPUT(GPIOK_PIN10) |          \
                                     PIN_MODE_INPUT(GPIOK_PIN11) |          \
                                     PIN_MODE_INPUT(GPIOK_PIN12) |          \
                                     PIN_MODE_INPUT(GPIOK_PIN13) |          \
                                     PIN_MODE_INPUT(GPIOK_PIN14) |          \
                                     PIN_MODE_INPUT(GPIOK_PIN15))
#define VAL_GPIOK_OTYPER            (PIN_OTYPE_PUSHPULL(GPIOK_PIN0) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOK_PIN1) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOK_PIN2) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOK_PIN3) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOK_PIN4) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOK_PIN5) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOK_PIN6) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOK_PIN7) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOK_PIN8) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOK_PIN9) |       \
                                     PIN_OTYPE_PUSHPULL(GPIOK_PIN10) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOK_PIN11) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOK_PIN12) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOK_PIN13) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOK_PIN14) |      \
                                     PIN_OTYPE_PUSHPULL(GPIOK_PIN15))
#define VAL_GPIOK_OSPEEDR           (PIN_OSPEED_VERYLOW(GPIOK_PIN0) |       \
                                     PIN_OSPEED_VERYLOW(GPIOK_PIN1) |       \
                                     PIN_OSPEED_VERYLOW(GPIOK_PIN2) |       \
                                     PIN_OSPEED_VERYLOW(GPIOK_PIN3) |       \
                                     PIN_OSPEED_VERYLOW(GPIOK_PIN4) |       \
                                     PIN_OSPEED_VERYLOW(GPIOK_PIN5) |       \
                                     PIN_OSPEED_VERYLOW(GPIOK_PIN6) |       \
                                     PIN_OSPEED_VERYLOW(GPIOK_PIN7) |       \
                                     PIN_OSPEED_VERYLOW(GPIOK_PIN8) |       \
                                     PIN_OSPEED_VERYLOW(GPIOK_PIN9) |       \
                                     PIN_OSPEED_VERYLOW(GPIOK_PIN10) |      \
                                     PIN_OSPEED_VERYLOW(GPIOK_PIN11) |      \
                                     PIN_OSPEED_VERYLOW(GPIOK_PIN12) |      \
                                     PIN_OSPEED_VERYLOW(GPIOK_PIN13) |      \
                                     PIN_OSPEED_VERYLOW(GPIOK_PIN14) |      \
                                     PIN_OSPEED_VERYLOW(GPIOK_PIN15))
#define VAL_GPIOK_PUPDR             (PIN_PUPDR_PULLUP(GPIOK_PIN0) |         \
                                     PIN_PUPDR_PULLUP(GPIOK_PIN1) |         \
                                     PIN_PUPDR_PULLUP(GPIOK_PIN2) |         \
                                     PIN_PUPDR_PULLUP(GPIOK_PIN3) |         \
                                     PIN_PUPDR_PULLUP(GPIOK_PIN4) |         \
                                     PIN_PUPDR_PULLUP(GPIOK_PIN5) |         \
                                     PIN_PUPDR_PULLUP(GPIOK_PIN6) |         \
                                     PIN_PUPDR_PULLUP(GPIOK_PIN7) |         \
                                     PIN_PUPDR_PULLUP(GPIOK_PIN8) |         \
                                     PIN_PUPDR_PULLUP(GPIOK_PIN9) |         \
                                     PIN_PUPDR_PULLUP(GPIOK_PIN10) |        \
                                     PIN_PUPDR_PULLUP(GPIOK_PIN11) |        \
                                     PIN_PUPDR_PULLUP(GPIOK_PIN12) |        \
                                     PIN_PUPDR_PULLUP(GPIOK_PIN13) |        \
                                     PIN_PUPDR_PULLUP(GPIOK_PIN14) |        \
                                     PIN_PUPDR_PULLUP(GPIOK_PIN15))
#define VAL_GPIOK_ODR               (PIN_ODR_HIGH(GPIOK_PIN0) |             \
                                     PIN_ODR_HIGH(GPIOK_PIN1) |             \
                                     PIN_ODR_HIGH(GPIOK_PIN2) |             \
                                     PIN_ODR_HIGH(GPIOK_PIN3) |             \
                                     PIN_ODR_HIGH(GPIOK_PIN4) |             \
                                     PIN_ODR_HIGH(GPIOK_PIN5) |             \
                                     PIN_ODR_HIGH(GPIOK_PIN6) |             \
                                     PIN_ODR_HIGH(GPIOK_PIN7) |             \
                                     PIN_ODR_HIGH(GPIOK_PIN8) |             \
                                     PIN_ODR_HIGH(GPIOK_PIN9) |             \
                                     PIN_ODR_HIGH(GPIOK_PIN10) |            \
                                     PIN_ODR_HIGH(GPIOK_PIN11) |            \
                                     PIN_ODR_HIGH(GPIOK_PIN12) |            \
                                     PIN_ODR_HIGH(GPIOK_PIN13) |            \
                                     PIN_ODR_HIGH(GPIOK_PIN14) |            \
                                     PIN_ODR_HIGH(GPIOK_PIN15))
#define VAL_GPIOK_AFRL              (PIN_AFIO_AF(GPIOK_PIN0, 0U) |          \
                                     PIN_AFIO_AF(GPIOK_PIN1, 0U) |          \
                                     PIN_AFIO_AF(GPIOK_PIN2, 0U) |          \
                                     PIN_AFIO_AF(GPIOK_PIN3, 0U) |          \
                                     PIN_AFIO_AF(GPIOK_PIN4, 0U) |          \
                                     PIN_AFIO_AF(GPIOK_PIN5, 0U) |          \
                                     PIN_AFIO_AF(GPIOK_PIN6, 0U) |          \
                                     PIN_AFIO_AF(GPIOK_PIN7, 0U))
#define VAL_GPIOK_AFRH              (PIN_AFIO_AF(GPIOK_PIN8, 0U) |          \
                                     PIN_AFIO_AF(GPIOK_PIN9, 0U) |          \
                                     PIN_AFIO_AF(GPIOK_PIN10, 0U) |         \
                                     PIN_AFIO_AF(GPIOK_PIN11, 0U) |         \
                                     PIN_AFIO_AF(GPIOK_PIN12, 0U) |         \
                                     PIN_AFIO_AF(GPIOK_PIN13, 0U) |         \
                                     PIN_AFIO_AF(GPIOK_PIN14, 0U) |         \
                                     PIN_AFIO_AF(GPIOK_PIN15, 0U))

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if !defined(_FROM_ASM_)
#ifdef __cplusplus
extern "C" {
#endif
  void boardInit(void);
#ifdef __cplusplus
}
#endif
#endif /* _FROM_ASM_ */

#endif /* BOARD_H */
