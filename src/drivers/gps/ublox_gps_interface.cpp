//
// Created by Clemens Elflein on 14.10.22.
// Copyright (c) 2022 Clemens Elflein. All rights reserved.
//

#include "ublox_gps_interface.h"

#include <ulog.h>

#include <WGS84toCartesian.hpp>
#include <chrono>
#include <cmath>

namespace xbot {
namespace driver {
namespace gps {

bool UbxGpsInterface::send_packet(uint8_t *frame, size_t size) {
  frame[0] = 0xb5;
  frame[1] = 0x62;
  auto *length_ptr = reinterpret_cast<uint16_t *>(frame + 4);
  *length_ptr = size - 8;

  uint8_t ck_a, ck_b;
  calculate_checksum(frame + 2, size - 4, ck_a, ck_b);

  frame[size - 2] = ck_a;
  frame[size - 1] = ck_b;

  return send_raw(frame, size);
}

/**
 * parses the buffer and returns how many more bytes to read
 */
size_t UbxGpsInterface::process_bytes(const uint8_t *buffer, size_t len) {
  static int invocations = 0;
  static int success = 0;
  static int error = 0;
  invocations++;
  while (!found_header_) {
    if (len == 0) {
      // request a byte
      return 1;
    }
    switch (gbuffer_fill) {
      case 0:
        // buffer empty, looking for 0xb5
        if (buffer[0] == 0xb5) {
          gbuffer[gbuffer_fill++] = *buffer;
        }
        buffer++;
        len--;
        break;
      case 1:
        // we have one byte, looking for 0x62
        if (buffer[0] == 0x62) {
          gbuffer[gbuffer_fill++] = *buffer;
          found_header_ = true;
        } else {
          // we had the 0xb5 but didn't get 0x62, reset to searching 0xb5
          gbuffer_fill = 0;
        }
        buffer++;
        len--;
        break;
      default:
        // just skip the byte and reset the buffer
        gbuffer_fill = 0;
        buffer++;
        len--;
        break;
    }
  }

  size_t bytes_to_take = etl::min(len, sizeof(gbuffer) - gbuffer_fill);
  memcpy(&gbuffer[gbuffer_fill], buffer, bytes_to_take);
  gbuffer_fill += bytes_to_take;
  len -= bytes_to_take;

  if (gbuffer_fill >= 6) {
    // get the length first to check, if we already got enough bytes
    uint16_t payload_length = gbuffer[5] << 8 | gbuffer[4];
    uint16_t total_length = payload_length + 8;

    if (total_length > sizeof(gbuffer)) {
      // cannot read whole packet, so probably error in size, skip to next
      found_header_ = false;
      gbuffer_fill = 0;
      return 1;
    }

    if (total_length > gbuffer_fill) {
      // fetch more data. bonus: we know exactly how many bytes
      return total_length - gbuffer_fill;
    }

    if (!validate_checksum(gbuffer, total_length)) {
      // invalid packet, reset header
      found_header_ = false;
      gbuffer_fill = 0;
      error++;
      return 1;
    }
    if (gbuffer_fill > 4) {
      process_ubx_packet(gbuffer + 2, gbuffer_fill - 4);
      success++;
    }

    // TODO: check, if there is a new packet already started in the buffer
    // this is currently not happening, because we don't request more bytes than
    // needed and made sure that the driver respects that request.
    // If the driver would push additional bytes, we'd lose packets.
    found_header_ = false;
    gbuffer_fill = 0;
  }

  // get the next byte
  return 1;
}

bool UbxGpsInterface::validate_checksum(const uint8_t *packet, size_t size) {
  uint8_t ck_a, ck_b;
  calculate_checksum(packet + 2, size - 4, ck_a, ck_b);

  bool valid = packet[size - 2] == ck_a && packet[size - 1] == ck_b;

  if (!valid) {
    ULOG_WARNING("got ubx packet with invalid checksum");
  }

  return valid;
}

void UbxGpsInterface::process_ubx_packet(const uint8_t *data,
                                         const size_t &size) {

  // data = no header bytes (starts with class) and stops before checksum

  uint16_t packet_id = data[0] << 8 | data[1];
  if (packet_id == (UbxNavPvt::CLASS_ID << 8 | UbxNavPvt::MESSAGE_ID)) {
    // substract class, id and length
    if (size - 4 == sizeof(UbxNavPvt)) {
      const auto *msg = reinterpret_cast<const UbxNavPvt *>(data + 4);
      SEGGER_SYSVIEW_MarkStart(1);
      handle_nav_pvt(msg);
      SEGGER_SYSVIEW_MarkStop(1);
    } else {
      ULOG_WARNING("size mismatch for PVT message!");
    }
  }
}

void UbxGpsInterface::handle_nav_pvt(const UbxNavPvt *msg) {

  // We have received a nav pvt message, copy to GPS state
  // check, if message is even roughly valid. If not - ignore it.
  bool gnssFixOK = (msg->flags & 0b0000001);
  bool invalidLlh = (msg->flags3 & 0b1);

  if (!gnssFixOK) {
    gps_state_valid_ = false;
    ULOG_WARNING("invalid gnssFix - dropping message");
    return;
  }
  if (invalidLlh) {
    gps_state_valid_ = false;
    ULOG_WARNING("invalid lat, lon, height - dropping message");
    return;
  }

  switch (msg->fixType) {
    case 1:
      gps_state_.fix_type = GpsState::FixType::DR_ONLY;
      break;
    case 2:
      gps_state_.fix_type = GpsState::FixType::FIX_2D;
      break;
    case 3:
      gps_state_.fix_type = GpsState::FixType::FIX_3D;
      break;
    case 4:
      gps_state_.fix_type = GpsState::FixType::GNSS_DR_COMBINED;
      break;
    default:
      gps_state_.fix_type = GpsState::FixType::NO_FIX;
      break;
  }

  bool diffSoln = (msg->flags & 0b0000010) >> 1;
  auto carrSoln = (uint8_t)((msg->flags & 0b11000000) >> 6);
  if (diffSoln) {
    switch (carrSoln) {
      case 1:
        gps_state_.rtk_type = GpsState::RTK_FLOAT;
        break;
      case 2:
        gps_state_.rtk_type = GpsState::RTK_FIX;
        break;
      default:
        gps_state_.rtk_type = GpsState::RTK_NONE;
        break;
    }
  } else {
    gps_state_.rtk_type = GpsState::RTK_NONE;
  }

  // Calculate the position
  double lat = (double)msg->lat / 10000000.0;
  double lon = (double)msg->lon / 10000000.0;
  double u = (double)msg->hMSL / 1000.0;
  const auto ne = wgs84::toCartesian({datum_lat_, datum_long_}, {lat, lon});

  gps_state_.pos_lat = lat;
  gps_state_.pos_lon = lon;
  gps_state_.position_valid = true;
  gps_state_.pos_e = ne[1];
  gps_state_.pos_n = ne[0];
  gps_state_.pos_u = u - datum_u_;
  gps_state_.position_accuracy = (double)sqrt(
      pow((double)msg->hAcc / 1000.0, 2) + pow((double)msg->vAcc / 1000.0, 2));

  gps_state_.vel_e = msg->velE / 1000.0;
  gps_state_.vel_n = msg->velN / 1000.0;
  gps_state_.vel_u = -msg->velD / 1000.0;

  double headAcc = (msg->headAcc / 100000.0) * (M_PI / 180.0);

  double hedVeh = msg->headVeh / 100000.0;
  hedVeh = -hedVeh * (M_PI / 180.0);
  hedVeh = fmod(hedVeh + (M_PI_2), 2.0 * M_PI);
  while (hedVeh < 0) {
    hedVeh += M_PI * 2.0;
  }

  double headMotion = msg->headMot / 100000.0;
  headMotion = -headMotion * (M_PI / 180.0);
  headMotion = fmod(headMotion + (M_PI_2), 2.0 * M_PI);
  while (headMotion < 0) {
    headMotion += M_PI * 2.0;
  }

  // There's no flag for that. Assume it's good
  gps_state_.motion_heading_valid = true;
  gps_state_.motion_heading = headMotion;
  gps_state_.motion_heading_accuracy = headAcc;

  // headAcc is the same for both
  gps_state_.vehicle_heading_valid = (msg->flags & 0b100000) >> 5;
  gps_state_.vehicle_heading_accuracy = headAcc;
  gps_state_.vehicle_heading = hedVeh;

  gps_state_.sensor_time = msg->iTOW;
  gps_state_.received_time = 0;

  gps_state_valid_ = true;

  if (state_callback) state_callback(gps_state_);
}

void UbxGpsInterface::calculate_checksum(const uint8_t *packet, size_t size,
                                         uint8_t &ck_a, uint8_t &ck_b) {
  ck_a = 0;
  ck_b = 0;

  for (size_t i = 0; i < size; i++) {
    ck_a += packet[i];
    ck_b += ck_a;
  }
}

UbxGpsInterface::UbxGpsInterface() {
}

void UbxGpsInterface::reset_parser_state() { found_header_ = false; }

}  // namespace gps
}  // namespace driver
}  // namespace xbot
