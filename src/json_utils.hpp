#ifndef XBOT_JSON_UTILS_HPP
#define XBOT_JSON_UTILS_HPP

#include <cstddef>
#include <cstdint>

namespace xbot::json {

// Escapes string as JSON string value.
// - Writes a NUL-terminated string to `out`.
// - Returns number of bytes written (excluding NUL).
// - On truncation, the output is still NUL-terminated and the returned length
//   is the number of bytes actually written.
inline size_t EscapeStringInto(const char* in, char* out, size_t out_cap) {
  if (out == nullptr || out_cap == 0) return 0;
  out[0] = '\0';
  if (in == nullptr) return 0;

  size_t w = 0;
  for (size_t r = 0; in[r] != '\0'; r++) {
    const uint8_t c = static_cast<uint8_t>(in[r]);

    const char* repl = nullptr;
    char tmp[7] = {0};

    switch (c) {
      case '"': repl = "\\\""; break;
      case '\\': repl = "\\\\"; break;
      case '\b': repl = "\\b"; break;
      case '\f': repl = "\\f"; break;
      case '\n': repl = "\\n"; break;
      case '\r': repl = "\\r"; break;
      case '\t': repl = "\\t"; break;
      default:
        if (c < 0x20U) {
          static const char kHex[] = "0123456789ABCDEF";
          tmp[0] = '\\';
          tmp[1] = 'u';
          tmp[2] = '0';
          tmp[3] = '0';
          tmp[4] = kHex[(c >> 4U) & 0x0FU];
          tmp[5] = kHex[c & 0x0FU];
          tmp[6] = '\0';
          repl = tmp;
        } else {
          tmp[0] = static_cast<char>(c);
          tmp[1] = '\0';
          repl = tmp;
        }
        break;
    }

    for (size_t i = 0; repl[i] != '\0'; i++) {
      if (w + 1 >= out_cap) {
        out[w] = '\0';
        return w;
      }
      out[w++] = repl[i];
    }
  }

  out[w] = '\0';
  return w;
}

}  // namespace xbot::json

#endif  // XBOT_JSON_UTILS_HPP
