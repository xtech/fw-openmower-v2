#ifndef LOOKUP_HPP
#define LOOKUP_HPP

#include <etl/crc16_genibus.h>

#include <cstdint>
#include <cstring>

constexpr uint16_t crc16(const char* str) {
  return etl::crc16_genibus(str, str + strlen(str)).value();
}

template <typename T, size_t N>
constexpr bool HasUniqueCrcs(const T (&arr)[N]) {
  for (size_t i = 0; i < N; i++) {
    for (size_t j = i + 1; j < N; j++) {
      if (arr[i].crc == arr[j].crc) {
        return false;
      }
    }
  }
  return true;
}

template <typename T, size_t N>
const T* LookupByName(const char* name, const T (&arr)[N]) {
  uint16_t crc = crc16(name);
  for (const auto& item : arr) {
    if (item.crc == crc) {
      return &item;
    }
  }
  return nullptr;
}

#endif  // LOOKUP_HPP
