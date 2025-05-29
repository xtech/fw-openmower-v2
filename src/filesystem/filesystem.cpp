//
// Created by clemens on 27.05.25.
//

#include "filesystem.hpp"

#include "ch.h"
#include "hal.h"
#include "hal_serial_nor.h"
#include "lfs.h"

static uint8_t lfs_read_buffer[FS_CACHE_SIZE];
static uint8_t lfs_write_buffer[FS_CACHE_SIZE];
static uint8_t lfs_lookahead_buffer[FS_LOOKAHEAD_SIZE];
static MUTEX_DECL(flash_mutex_);

const WSPIConfig WSPIcfg1 = {.end_cb = nullptr,
                             .error_cb = nullptr,
                             .dcr1 = STM32_DCR1_MTYP(2) | STM32_DCR1_DEVSIZE(23U) | /* 16MB device.          */
                                     STM32_DCR1_CSHT(1U) /* NCS 2 cycles delay.  */,
                             .dcr2 = 0,
                             .dcr3 = 0,
                             .dcr4 = 0};

static const SNORConfig snorcfg1 = {.busp = &WSPID1, .buscfg = &WSPIcfg1};
static SNORDriver snor1;
static snor_nocache_buffer_t snor_buffer;

lfs_t lfs;

static int read_flash(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size) {
  if (snor_device_read(&snor1, block * c->block_size + off, size, static_cast<uint8_t *>(buffer)) == FLASH_NO_ERROR) {
    return LFS_ERR_OK;
  }
  return LFS_ERR_IO;
}

static int write_flash(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer,
                       lfs_size_t size) {
  if (snor_device_program(&snor1, block * c->block_size + off, size, static_cast<const uint8_t *>(buffer)) ==
      FLASH_NO_ERROR) {
    return LFS_ERR_OK;
  }

  return LFS_ERR_IO;
}

static int erase_flash(const struct lfs_config *c, lfs_block_t block) {
  (void)c;
  // This works, because block size == sector size
  if (snor_device_start_erase_sector(&snor1, block) != FLASH_NO_ERROR) {
    return LFS_ERR_IO;
  }

  for (uint32_t wait = 0; snor_device_query_erase(&snor1, &wait) != FLASH_NO_ERROR;) {
    chThdSleep(TIME_MS2I(wait));
  }
  return LFS_ERR_OK;
}

static int sync_flash(const struct lfs_config *c) {
  (void)c;
  // Noop, we don't cache in the driver
  return LFS_ERR_OK;
}

static int lock_flash(const struct lfs_config *c) {
  (void)c;
  chMtxLock(&flash_mutex_);
  return LFS_ERR_OK;
}

static int unlock_flash(const struct lfs_config *c) {
  (void)c;
  chMtxUnlock(&flash_mutex_);
  return LFS_ERR_OK;
}

const static lfs_config cfg = {.context = nullptr,
                               .read = read_flash,
                               .prog = write_flash,
                               .erase = erase_flash,
                               .sync = sync_flash,
                               .lock = lock_flash,
                               .unlock = unlock_flash,
                               // block device configuration
                               .read_size = 16,
                               .prog_size = 256,
                               // make sure this is aligned with the flash sector size!
                               .block_size = 4096,
                               .block_count = 4096,
                               .block_cycles = 500,
                               .cache_size = FS_CACHE_SIZE,
                               .lookahead_size = FS_LOOKAHEAD_SIZE,
                               .compact_thresh = 0,
                               .read_buffer = lfs_read_buffer,
                               .prog_buffer = lfs_write_buffer,
                               .lookahead_buffer = lfs_lookahead_buffer,
                               .name_max = 0,
                               .file_max = 0,
                               .attr_max = 0,
                               .metadata_max = 0,
                               .inline_max = 0};

bool InitFS() {
  wspiStart(&WSPID1, &WSPIcfg1);

  snorObjectInit(&snor1, &snor_buffer);
  snorStart(&snor1, &snorcfg1);

  // mount the filesystem
  int err = lfs_mount(&lfs, &cfg);

  // reformat if we can't mount the filesystem
  // this should only happen on the first boot
  if (err) {
    lfs_format(&lfs, &cfg);
    err = lfs_mount(&lfs, &cfg);
  }

  return err == LFS_ERR_OK;
}
