//
// Created by clemens on 27.05.25.
//

#ifndef FILE_HPP
#define FILE_HPP

#include "filesystem.hpp"

class File {
 public:
  File() = default;

  ~File() {
    if (is_open) {
      close();
    }
  }

  int open(const char* path, int flags = LFS_O_RDWR | LFS_O_CREAT) {
    if (is_open) return LFS_ERR_INVAL;
    file_config_ = {};
    file_config_.buffer = cache_buffer_;
    auto result = lfs_file_opencfg(&lfs, &file_, path, flags, &file_config_);
    is_open = result == LFS_ERR_OK;
    return result;
  }

  void close() {
    if (is_open) {
      lfs_file_close(&lfs, &file_);
      is_open = false;
    }
  }

  int read(void* target, size_t size) {
    return lfs_file_read(&lfs, &file_, target, size);
  }

  int write(void* buffer, size_t size) {
    return lfs_file_write(&lfs, &file_, buffer, size);
  }

  int sync() {
    return lfs_file_sync(&lfs, &file_);
  }

  int rewind() {
    return lfs_file_rewind(&lfs, &file_);
  }

  int seek(lfs_soff_t off, int whence) {
    return lfs_file_seek(&lfs, &file_, off, whence);
  }

  [[nodiscard]] bool isOpen() const {
    return is_open;
  }

 private:
  lfs_file_t file_{};
  lfs_file_config file_config_{};
  uint8_t cache_buffer_[FS_CACHE_SIZE]{};
  bool is_open = false;
};

#endif  // FILE_HPP
