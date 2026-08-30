//
// Corruption / checksum test interface.
//
// Opens a UDP socket and validates incoming test packets, replying with a
// verdict. Used together with tools/net_corruption_test/send_corrupt.py to
// characterise whether the network stack (lwIP + MAC) drops or delivers
// corrupted packets. See that script for the wire format and test cases.
//

#ifndef CHECKSUM_TEST_INTERFACE_HPP
#define CHECKSUM_TEST_INTERFACE_HPP

#include <cstdint>

// UDP port the firmware listens on for corruption test packets.
static constexpr uint16_t CHECKSUM_TEST_PORT = 4500;

// Starts the checksum test thread. Safe to call once after the network stack
// (xbot::service::Io::start()) is up.
void InitChecksumTestInterface();

#endif  // CHECKSUM_TEST_INTERFACE_HPP
