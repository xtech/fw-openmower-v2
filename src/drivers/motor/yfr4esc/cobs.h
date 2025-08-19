// SPDX-License-Identifier: MIT
// Minimal COBS (Consistent Overhead Byte Stuffing) encode/decode helpers
// Reference: COBS specification

#pragma once

#include <stddef.h>
#include <stdint.h>

// Header-only minimal COBS helpers (MIT licensed).
// Encode 'input' into 'output'. Returns number of bytes written.
// Input must NOT include the 0x00 frame delimiter. Caller should append 0x00.
static inline size_t cobs_encode(const uint8_t *input, size_t length, uint8_t *output) {
  const uint8_t *end = input + length;
  uint8_t *out = output;
  uint8_t *code_ptr = out++;
  uint8_t code = 1;

  while (input < end) {
    if (*input != 0) {
      *out++ = *input++;
      if (++code == 0xFF) {
        *code_ptr = code;
        code_ptr = out++;
        code = 1;
      }
    } else {
      *code_ptr = code;
      code_ptr = out++;
      code = 1;
      ++input;  // skip zero byte
    }
  }

  *code_ptr = code;
  return (size_t)(out - output);
}

// Decode 'input' into 'output'. Returns number of bytes written, or 0 on error.
// Input must NOT include the trailing 0x00 frame delimiter.
static inline size_t cobs_decode(const uint8_t *input, size_t length, uint8_t *output) {
  const uint8_t *end = input + length;
  uint8_t *out = output;
  while (input < end) {
    uint8_t code = *input++;
    if (code == 0) return 0;  // invalid
    for (uint8_t i = 1; i < code; ++i) {
      if (input >= end) return 0;
      *out++ = *input++;
    }
    if (code != 0xFF && input < end) {
      *out++ = 0;
    }
  }
  return (size_t)(out - output);
}
