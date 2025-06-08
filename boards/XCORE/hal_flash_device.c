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
 * @file    hal_flash_device.c
 * @brief   Micron W25Q serial flash driver code.
 *
 * @addtogroup WINBOND_W25Q
 * @{
 */

#include <string.h>

#include "hal.h"
#include "hal_serial_nor.h"

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

#define PAGE_SIZE 256U
#define PAGE_MASK (PAGE_SIZE - 1U)

#if W25Q_USE_SUB_SECTORS == TRUE
/* 4 KB */
#define SECTOR_SIZE 0x00001000U
#define CMD_SECTOR_ERASE W25Q_CMD_SECTOR_ERASE
#else
/* 64 KB */
#define SECTOR_SIZE 0x00010000U
#define CMD_SECTOR_ERASE W25Q_CMD_64K_BLOCK_ERASE
#endif

// Local define, since we don't want to use the header ones
#define WSPI_CFG_CMD                                                                                         \
  (WSPI_CFG_CMD_MODE_ONE_LINE | WSPI_CFG_ADDR_MODE_NONE | WSPI_CFG_ALT_MODE_NONE | WSPI_CFG_DATA_MODE_NONE | \
   WSPI_CFG_CMD_SIZE_8)

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

/**
 * @brief   W25Q128 descriptor.
 */
flash_descriptor_t snor_descriptor = {
    .attributes = FLASH_ATTR_ERASED_IS_ONE | FLASH_ATTR_REWRITABLE | FLASH_ATTR_SUSPEND_ERASE_CAPABLE,
    .page_size = 256U,
    .sectors_count = 0U, /* It is overwritten.*/
    .sectors = NULL,
    .sectors_size = SECTOR_SIZE,
    .address = 0U,
    .size = 0U /* It is overwritten.*/

};

#if (WSPI_SUPPORTS_MEMMAP == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   Fast read command for memory mapped mode.
 */
const wspi_command_t snor_memmap_read = {.cmd = W25Q_CMD_FAST_READ_QUAD_IO,
                                         .addr = 0,
                                         .dummy = W25Q_READ_DUMMY_CYCLES,
                                         .cfg = WSPI_CFG_ADDR_SIZE_24 | WSPI_CFG_CMD_MODE_ONE_LINE |
                                                WSPI_CFG_ADDR_MODE_FOUR_LINES | WSPI_CFG_DATA_MODE_FOUR_LINES |
                                                WSPI_CFG_ALT_MODE_FOUR_LINES | /* Always 4 lines, note.*/
                                                WSPI_CFG_ALT_SIZE_8 | WSPI_CFG_SIOO};
#endif

/*===========================================================================*/
/* Driver local variables and types.                                         */
/*===========================================================================*/

/* Initial W25Q_CMD_READ_ID command.*/
static const wspi_command_t w25q_cmd_read_id = {.cmd = W25Q_CMD_READ_JEDEC_ID,
                                                .cfg = WSPI_CFG_CMD_MODE_ONE_LINE | WSPI_CFG_DATA_MODE_ONE_LINE,
                                                .addr = 0,
                                                .alt = 0,
                                                .dummy = 0};

static const wspi_command_t write_enable_cmd = {.cmd = W25Q_CMD_WRITE_ENABLE, .cfg = WSPI_CFG_CMD_MODE_ONE_LINE, .addr = 0, .alt = 0, .dummy = 0};
static const wspi_command_t read_status_register_cmd = {.cmd = W25Q_CMD_READ_STATUS_REGISTER, .cfg = WSPI_CFG_CMD_MODE_ONE_LINE | WSPI_CFG_DATA_MODE_ONE_LINE, .addr = 0, .alt = 0, .dummy = 0};


/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

static bool w25q_find_id(const uint8_t *set, size_t size, uint8_t element) {
  size_t i;

  for (i = 0; i < size; i++) {
    if (set[i] == element) {
      return true;
    }
  }
  return false;
}

static flash_error_t w25q_poll_status(SNORDriver *devp) {
  int timeout = 100;

  do {
    /* Read status command.*/
    wspiReceive(devp->config->busp, &read_status_register_cmd, 1, &devp->nocache->buf[0]);
    if ((devp->nocache->buf[0] & W25Q_FLAGS_BUSY) == 0U) {
      break;
    }
#if W25Q_NICE_WAITING == TRUE
    osalThreadSleepMilliseconds(1);
#endif
  } while (--timeout);

  if (timeout <= 0) {
    return FLASH_ERROR_PROGRAM;
  }

  /* Checking for errors.*/
  /* NOP */

  return FLASH_NO_ERROR;
}

static void w25q_reset_memory(SNORDriver *devp) {
  // Reset the Flash by sending reset enable -> reset
  wspi_command_t cmd = {.cmd = 0, .cfg = WSPI_CFG_CMD_MODE_ONE_LINE, .addr = 0, .alt = 0, .dummy = 0};
  cmd.cmd = W25Q_CMD_RESET_ENABLE;
  wspiCommand(devp->config->busp, &cmd);
  cmd.cmd = W25Q_CMD_RESET;
  wspiCommand(devp->config->busp, &cmd);
}

static const uint8_t w25q_manufacturer_ids[] = W25Q_SUPPORTED_MANUFACTURE_IDS;
static const uint8_t w25q_memory_type_ids[] = W25Q_SUPPORTED_MEMORY_TYPE_IDS;

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

void snor_device_init(SNORDriver *devp) {
  /* Attempting a reset of the device, it could be in an unexpected state
     because a CPU reset does not reset the memory too.*/
  w25q_reset_memory(devp);

  /* Reading device ID and unique ID.*/
  wspiReceive(devp->config->busp, &w25q_cmd_read_id, 3U, &devp->nocache->buf[0]);

  /* Checking if the device is white listed.*/
  osalDbgAssert(w25q_find_id(w25q_manufacturer_ids, sizeof w25q_manufacturer_ids, devp->nocache->buf[0]),
                "invalid manufacturer id");
  osalDbgAssert(w25q_find_id(w25q_memory_type_ids, sizeof w25q_memory_type_ids, devp->nocache->buf[1]),
                "invalid memory type id");

  /* Setting up the device size.*/
  snor_descriptor.sectors_count = (1U << (size_t)devp->nocache->buf[2]) / SECTOR_SIZE;
  snor_descriptor.size = (size_t)snor_descriptor.sectors_count * SECTOR_SIZE;
}

flash_error_t snor_device_read(SNORDriver *devp, flash_offset_t offset, size_t n, uint8_t *rp) {
  wspi_command_t cmd = {.cmd = W25Q_CMD_FAST_READ_QUAD_IO,
                     .cfg = (WSPI_CFG_CMD_MODE_ONE_LINE | WSPI_CFG_ADDR_MODE_FOUR_LINES | WSPI_CFG_ALT_MODE_FOUR_LINES | WSPI_CFG_ALT_SIZE_8 |
                             WSPI_CFG_DATA_MODE_FOUR_LINES | WSPI_CFG_CMD_SIZE_8 | WSPI_CFG_ADDR_SIZE_24),
                     .addr = offset,
                     .alt = 0xFF,
                     .dummy = 4};

  wspiReceive(devp->config->busp, &cmd, n, rp);

  return FLASH_NO_ERROR;
}

flash_error_t snor_device_program(SNORDriver *devp, flash_offset_t offset, size_t n, const uint8_t *pp) {
  wspi_command_t cmd = {.cmd = W25_QUAD_INPUT_PAGE_PROGRAM,
                   .cfg = (WSPI_CFG_CMD_MODE_ONE_LINE | WSPI_CFG_ADDR_MODE_ONE_LINE | WSPI_CFG_ALT_MODE_NONE |
                           WSPI_CFG_DATA_MODE_FOUR_LINES | WSPI_CFG_CMD_SIZE_8 | WSPI_CFG_ADDR_SIZE_24),
                   .addr = 0,
                   .alt = 0,
                   .dummy = 0};


  /* Data is programmed page by page.*/
  while (n > 0U) {
    flash_error_t err;

    /* Data size that can be written in a single program page operation.*/
    size_t chunk = (size_t)(((offset | PAGE_MASK) + 1U) - offset);
    if (chunk > n) {
      chunk = n;
    }

    /* Enabling write operation.*/
    wspiCommand(devp->config->busp, &write_enable_cmd);

    /* Page program command.*/
    cmd.addr = offset;
    wspiSend(devp->config->busp, &cmd, chunk, pp);

    /* Wait for status and check errors.*/
    err = w25q_poll_status(devp);
    if (err != FLASH_NO_ERROR) {
      return err;
    }

    /* Next page.*/
    offset += chunk;
    pp += chunk;
    n -= chunk;
  }

  return FLASH_NO_ERROR;
}

flash_error_t snor_device_start_erase_all(SNORDriver *devp) {
  wspi_command_t cmd = {.cmd = W25Q_CMD_BULK_ERASE, .cfg = WSPI_CFG_CMD_MODE_ONE_LINE, .addr = 0, .alt = 0, .dummy = 0};


  /* Enabling write operation.*/
  wspiCommand(devp->config->busp, &write_enable_cmd);

  /* Bulk erase command.*/
  wspiCommand(devp->config->busp, &cmd);

  return FLASH_NO_ERROR;
}

flash_error_t snor_device_start_erase_sector(SNORDriver *devp, flash_sector_t sector) {
  flash_offset_t offset = (flash_offset_t)(sector * SECTOR_SIZE);
  wspi_command_t cmd = {.cmd = CMD_SECTOR_ERASE, .cfg = WSPI_CFG_CMD_MODE_ONE_LINE | WSPI_CFG_ADDR_MODE_ONE_LINE | WSPI_CFG_ADDR_SIZE_24, .addr = offset, .alt = 0, .dummy = 0};

  /* Enabling write operation.*/
  wspiCommand(devp->config->busp, &write_enable_cmd);

  /* Sector erase command.*/
  wspiCommand(devp->config->busp, &cmd);

  return FLASH_NO_ERROR;
}

flash_error_t snor_device_verify_erase(SNORDriver *devp, flash_sector_t sector) {
  uint8_t cmpbuf[W25Q_COMPARE_BUFFER_SIZE];
  flash_offset_t offset;
  size_t n;

  /* Read command.*/
  offset = (flash_offset_t)(sector * SECTOR_SIZE);
  n = SECTOR_SIZE;
  while (n > 0U) {
    uint8_t *p;

    snor_device_read(devp, offset, sizeof(cmpbuf), cmpbuf);

    /* Checking for erased state of current buffer.*/
    for (p = cmpbuf; p < &cmpbuf[W25Q_COMPARE_BUFFER_SIZE]; p++) {
      if (*p != 0xFFU) {
        /* Ready state again.*/
        devp->state = FLASH_READY;

        return FLASH_ERROR_VERIFY;
      }
    }

    offset += sizeof cmpbuf;
    n -= sizeof cmpbuf;
  }

  return FLASH_NO_ERROR;
}

flash_error_t snor_device_query_erase(SNORDriver *devp, uint32_t *msec) {
  /* Read status command.*/
  wspiReceive(devp->config->busp, &read_status_register_cmd, 1, &devp->nocache->buf[0]);

  /* If the P/E bit is 1 (busy) report that the operation is still in progress.*/
  if ((devp->nocache->buf[0] & W25Q_FLAGS_BUSY) != 0U) {
    /* Recommended time before polling again, this is a simplified
       implementation.*/
    if (msec != NULL) {
      *msec = 1U;
    }

    return FLASH_BUSY_ERASING;
  }

  /* Checking for errors.*/
  /* NOP */

  return FLASH_NO_ERROR;
}

flash_error_t snor_device_read_sfdp(SNORDriver *devp, flash_offset_t offset, size_t n, uint8_t *rp) {
  (void)devp;
  (void)rp;
  (void)offset;
  (void)n;
  // Noop for this device
  return FLASH_NO_ERROR;
}

void snor_activate_xip(SNORDriver *devp) {
  (void)devp;
  // Noop for this device
}

void snor_reset_xip(SNORDriver *devp) {
  (void)devp;
  // Noop for this device
}

/** @} */
