//
// Created for network corruption / checksum testing.
//

#include "checksum_test_interface.hpp"

// clang-format off
#include "ch.h"
#include "hal.h"
// clang-format on

#include <cstddef>
#include <cstring>

#include "lwip/sockets.h"
#include "ulog.h"

namespace {

// Wire format. All fields little-endian (host is little-endian Cortex-M7, and
// the Linux test tool matches). __attribute__((packed)) to avoid padding.
constexpr uint32_t kTestMagic = 0x54534554;   // "TEST"
constexpr uint32_t kReplyMagic = 0x504C5052;  // "RPLP"

struct TestHeader {
  uint32_t magic;
  uint32_t test_id;
  uint32_t declared_len;  // expected payload length in bytes
  uint32_t crc32;         // CRC32 (IEEE) over the whole packet except this field
} __attribute__((packed));

struct ReplyHeader {
  uint32_t magic;
  uint32_t test_id;
  uint32_t verdict;             // Verdict enum
  uint32_t received_total_len;  // full datagram length as seen by recvfrom
  uint32_t computed_crc;        // CRC32 the firmware computed over the payload
} __attribute__((packed));

enum Verdict : uint32_t {
  VERDICT_OK = 0,
  VERDICT_BAD_MAGIC = 1,
  VERDICT_LEN_MISMATCH = 2,
  VERDICT_CRC_MISMATCH = 3,
  VERDICT_TOO_SHORT = 4,
};

// Standard reflected CRC32 (IEEE 802.3, poly 0xEDB88320), matching Python's
// binascii.crc32 / zlib.crc32. Computed without a table to keep things small.
// Streaming form: pass the running register in/out so a CRC can span multiple
// non-contiguous regions. Seed with 0xFFFFFFFF and finalise with ~crc.
uint32_t Crc32Feed(uint32_t crc, const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; bit++) {
      uint32_t mask = -(crc & 1u);
      crc = (crc >> 1) ^ (0xEDB88320u & mask);
    }
  }
  return crc;
}

const char *VerdictName(uint32_t v) {
  switch (v) {
    case VERDICT_OK: return "OK";
    case VERDICT_BAD_MAGIC: return "BAD_MAGIC";
    case VERDICT_LEN_MISMATCH: return "LEN_MISMATCH";
    case VERDICT_CRC_MISMATCH: return "CRC_MISMATCH";
    case VERDICT_TOO_SHORT: return "TOO_SHORT";
    default: return "UNKNOWN";
  }
}

// Receive buffer is intentionally larger than xbot::config::max_packet_size
// (1500) so that oversize / truncation behaviour is observable here instead of
// being silently clipped like the production path.
uint8_t rx_buffer[2048];

THD_WORKING_AREA(waChecksumTest, 2048);

void ChecksumTestThread(void *) {
  chRegSetThreadName("ChecksumTest");

  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    ULOG_ERROR("ChecksumTest: socket creation failed");
    return;
  }

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(CHECKSUM_TEST_PORT);

  if (bind(sockfd, reinterpret_cast<struct sockaddr *>(&server_addr), sizeof(server_addr)) < 0) {
    ULOG_ERROR("ChecksumTest: bind to port %u failed", CHECKSUM_TEST_PORT);
    lwip_close(sockfd);
    return;
  }

  ULOG_INFO("ChecksumTest: listening on UDP port %u", CHECKSUM_TEST_PORT);

  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);

  while (true) {
    const int received = recvfrom(sockfd, rx_buffer, sizeof(rx_buffer), 0,
                                  reinterpret_cast<struct sockaddr *>(&client_addr), &client_len);
    if (received < 0) {
      continue;
    }

    ReplyHeader reply;
    reply.magic = kReplyMagic;
    reply.received_total_len = static_cast<uint32_t>(received);
    reply.computed_crc = 0;
    reply.test_id = 0;
    reply.verdict = VERDICT_TOO_SHORT;

    if (static_cast<size_t>(received) >= sizeof(TestHeader)) {
      TestHeader hdr;
      memcpy(&hdr, rx_buffer, sizeof(hdr));
      reply.test_id = hdr.test_id;

      const size_t payload_len = static_cast<size_t>(received) - sizeof(TestHeader);
      // CRC covers the whole packet except the crc32 field itself: the header
      // bytes before the field, then the payload after the header.
      uint32_t crc = 0xFFFFFFFFu;
      crc = Crc32Feed(crc, rx_buffer, offsetof(TestHeader, crc32));
      crc = Crc32Feed(crc, rx_buffer + sizeof(TestHeader), payload_len);
      crc = ~crc;
      reply.computed_crc = crc;

      if (hdr.magic != kTestMagic) {
        reply.verdict = VERDICT_BAD_MAGIC;
      } else if (hdr.declared_len != payload_len) {
        reply.verdict = VERDICT_LEN_MISMATCH;
      } else if (hdr.crc32 != crc) {
        reply.verdict = VERDICT_CRC_MISMATCH;
      } else {
        reply.verdict = VERDICT_OK;
      }
    }

    ULOG_INFO("ChecksumTest: id=%u len=%d verdict=%s", reply.test_id, received, VerdictName(reply.verdict));

    sendto(sockfd, &reply, sizeof(reply), 0, reinterpret_cast<struct sockaddr *>(&client_addr), client_len);
  }
}

}  // namespace

void InitChecksumTestInterface() {
  thread_t *tp = chThdCreateStatic(waChecksumTest, sizeof(waChecksumTest), NORMALPRIO, ChecksumTestThread, nullptr);
  tp->name = "ChecksumTest";
}
