/*
    ChibiOS - Copyright (C) 2006..2018 Giovanni Di Sirio

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

/**
 * @file    hal_flash_device.h
 * @brief   Winbond W25Q serial flash driver header.
 *
 * @addtogroup WINBONT_N25Q
 * @{
 */

#ifndef HAL_FLASH_DEVICE_H
#define HAL_FLASH_DEVICE_H

#if (SNOR_BUS_DRIVER != SNOR_BUS_DRIVER_WSPI)
#error "This driver only works with WSPI bus"
#endif


/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/**
 * @name    Device capabilities
 * @{
 */
#define SNOR_DEVICE_SUPPORTS_XIP            TRUE
/** @} */

/**
 * @name    Device identification
 * @{
 */
#define W25Q_SUPPORTED_MANUFACTURE_IDS      {0xEF}
#define W25Q_SUPPORTED_MEMORY_TYPE_IDS      {0x40, 0x60}
/** @} */

/**
 * @name    Command codes
 * @{
 */
#define W25Q_CMD_RESET_ENABLE                       0x66
#define W25Q_CMD_RESET                              0x99
#define W25Q_CMD_READ_JEDEC_ID                      0x9F
#define W25Q_CMD_READ_SFDP_REGISTER                 0x5A
#define W25Q_CMD_READ                               0x03
#define W25Q_CMD_FAST_READ                          0x0B
#define W25Q_CMD_FAST_READ_QUAD_IO                  0xEB
#define W25Q_CMD_WRITE_ENABLE                       0x06
#define W25Q_CMD_WRITE_DISABLE                      0x04
#define W25Q_CMD_READ_STATUS_REGISTER               0x05
#define W25Q_CMD_WRITE_STATUS_REGISTER              0x01
#define W25Q_CMD_READ_STATUS_REGISTER_2             0x35
#define W25Q_CMD_PAGE_PROGRAM                       0x02
#define W25Q_CMD_SECTOR_ERASE                       0x20
#define W25Q_CMD_32K_BLOCK_ERASE                    0x53
#define W25Q_CMD_64K_BLOCK_ERASE                    0xD8
#define W25Q_CMD_BULK_ERASE                         0xC7
#define W25Q_CMD_PROGRAM_ERASE_RESUME               0x7A
#define W25Q_CMD_PROGRAM_ERASE_SUSPEND              0x75
#define W25Q_CMD_READ_UID_ARRAY                     0x4B
#define W25Q_CMD_PROGRAM_SECURITY_REGS              0x42
#define W25_QUAD_INPUT_PAGE_PROGRAM                 0x32
/** @} */

/**
 * @name    Flags status register bits
 * @{
 */
#define W25Q_FLAGS_BUSY                             0x01U
/** @} */

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @brief   Delays insertions.
 * @details If enabled this options inserts delays into the flash waiting
 *          routines releasing some extra CPU time for threads with lower
 *          priority, this may slow down the driver a bit however.
 */
#if !defined(W25Q_NICE_WAITING) || defined(__DOXYGEN__)
#define W25Q_NICE_WAITING                   TRUE
#endif

/**
 * @brief   Uses 4kB sub-sectors rather than 64kB sectors.
 */
#if !defined(W25Q_USE_SUB_SECTORS) || defined(__DOXYGEN__)
#define W25Q_USE_SUB_SECTORS                TRUE
#endif

/**
 * @brief   Size of the compare buffer.
 * @details This buffer is allocated in the stack frame of the function
 *          @p flashVerifyErase() and its size must be a power of two.
 *          Larger buffers lead to better verify performance but increase
 *          stack usage for that function.
 */
#if !defined(W25Q_COMPARE_BUFFER_SIZE) || defined(__DOXYGEN__)
#define W25Q_COMPARE_BUFFER_SIZE            32
#endif

/**
 * @brief   Number of dummy cycles for fast read (1..15).
 * @details This is the number of dummy cycles to be used for fast read
 *          operations.
 */
#if !defined(W25Q_READ_DUMMY_CYCLES) || defined(__DOXYGEN__)
#define W25Q_READ_DUMMY_CYCLES              4
#endif

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if (W25Q_COMPARE_BUFFER_SIZE & (W25Q_COMPARE_BUFFER_SIZE - 1)) != 0
#error "invalid W25Q_COMPARE_BUFFER_SIZE value"
#endif

#if (W25Q_READ_DUMMY_CYCLES < 1) || (W25Q_READ_DUMMY_CYCLES > 15)
#error "invalid W25Q_READ_DUMMY_CYCLES value (1..15)"
#endif


// We cannot use these "simplified" accesses, because they differ
// for each command on the W25Q, so best to zero it, to safe some frustration later
#define SNOR_WSPI_CFG_CMD               0
#define SNOR_WSPI_CFG_CMD_ADDR          0
#define SNOR_WSPI_CFG_CMD_DATA          0
#define SNOR_WSPI_CFG_CMD_ADDR_DATA     0

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Driver macros.                                                            */
/*===========================================================================*/

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if !defined(__DOXYGEN__)
extern flash_descriptor_t snor_descriptor;
#endif

#if WSPI_SUPPORTS_MEMMAP == TRUE
extern const wspi_command_t snor_memmap_read;
#endif

#ifdef __cplusplus
extern "C" {
#endif
  void snor_device_init(SNORDriver *devp);
  flash_error_t snor_device_read(SNORDriver *devp, flash_offset_t offset,
                                 size_t n, uint8_t *rp);
  flash_error_t snor_device_program(SNORDriver *devp, flash_offset_t offset,
                                    size_t n, const uint8_t *pp);
  flash_error_t snor_device_start_erase_all(SNORDriver *devp);
  flash_error_t snor_device_start_erase_sector(SNORDriver *devp,
                                               flash_sector_t sector);
  flash_error_t snor_device_verify_erase(SNORDriver *devp,
                                         flash_sector_t sector);
  flash_error_t snor_device_query_erase(SNORDriver *devp, uint32_t *msec);
  flash_error_t snor_device_read_sfdp(SNORDriver *devp, flash_offset_t offset,
                                    size_t n, uint8_t *rp);
#if (SNOR_BUS_DRIVER == SNOR_BUS_DRIVER_WSPI) &&                            \
    (SNOR_DEVICE_SUPPORTS_XIP == TRUE)
  void snor_activate_xip(SNORDriver *devp);
  void snor_reset_xip(SNORDriver *devp);
#endif
#ifdef __cplusplus
}
#endif

#endif /* HAL_FLASH_DEVICE_H */

/** @} */

