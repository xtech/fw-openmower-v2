#include "nmea_gps_driver.h"

#include "minmea.h"

namespace xbot::driver::gps {

/**
 * parses the buffer and returns how many more bytes to read
 */
size_t NmeaGpsDriver::ProcessBytes(const uint8_t* buffer, size_t len) {
  static int invocations = 0;
  static int success = 0;
  static int error = 0;
  invocations++;
  while (len > 0) {
    // If we have no partial line yet, look for the first dollar symbol, ignoring everything before it.
    if (line_len == 0) {
      const uint8_t* dollar = (const uint8_t*)memchr(buffer, '$', len);
      if (dollar != nullptr) {
        len -= dollar - buffer;
        buffer = dollar;
      } else {
        return 1;
      }
    }

    // We have the start of the line now. Either it's in line[0] or in *buffer.
    // Search for the end of the line.
    const uint8_t* newline = (const uint8_t*)memchr(buffer, '\n', len);
    if (newline != nullptr) {
      size_t bytes_to_take = newline - buffer + 1;
      if (line_len + bytes_to_take + 1 <= sizeof(line)) {
        memcpy(&line[line_len], buffer, bytes_to_take);
        line_len += bytes_to_take;
        line[line_len] = '\0';
        if (ProcessLine(line)) {
          success++;
        } else {
          error++;
        }
      } else {
        // Line would be too long, so throw away the buffer.
        error++;
      }
      len -= bytes_to_take;
      buffer = newline + 1;
      line_len = 0;
    } else {
      // We will at least need two more characters (newline and null-byte), check if it could fit.
      if (line_len + len + 2 <= sizeof(line)) {
        // Yes, so copy the remaining bytes for next time.
        memcpy(&line[line_len], buffer, len);
        return 1;
      } else {
        error++;
        line_len = 0;
      }
    }
  }
  return 1;
}

void NmeaGpsDriver::UpdateGpsStateValidity() {
  if (gps_state_.fix_type != GpsState::FIX_2D && gps_state_.fix_type != GpsState::FIX_3D) {
    gps_state_valid_ = false;
  } else if (fix_quality == 0) {
    gps_state_valid_ = false;
  } else {
    gps_state_valid_ = true;
  }
}

bool NmeaGpsDriver::ProcessLine(const char* line) {
  switch (minmea_sentence_id(line, true)) {
    case MINMEA_SENTENCE_GGA: {
      struct minmea_sentence_gga gga;
      if (!minmea_parse_gga(&gga, line)) {
        return false;
      }

      gps_state_.pos_lat = minmea_tocoord(&gga.latitude);
      gps_state_.pos_lon = minmea_tocoord(&gga.longitude);
      gps_state_.pos_height = minmea_tofloat(&gga.altitude);
      gps_state_.position_valid = true;

      switch (gga.fix_quality) {
        case 5: gps_state_.rtk_type = GpsState::RTK_FLOAT; break;
        case 4: gps_state_.rtk_type = GpsState::RTK_FIX; break;
        default: gps_state_.rtk_type = GpsState::RTK_NONE; break;
      }
      fix_quality = gga.fix_quality;

      // Set number of satellites used from GGA message
      gps_state_.num_sv = gga.satellites_tracked;

      UpdateGpsStateValidity();
      TriggerStateCallback();
      return true;
    }

    case MINMEA_SENTENCE_RMC: {
      struct minmea_sentence_rmc rmc;
      if (!minmea_parse_rmc(&rmc, line)) {
        return false;
      }

      struct timespec ts;
      if (minmea_gettime(&ts, &rmc.date, &rmc.time) != 0) {
        return false;
      }
      gps_state_.sensor_time = ts.tv_sec;
      gps_state_.received_time = ts.tv_sec;

      gps_state_.pos_lat = minmea_tocoord(&rmc.latitude);
      gps_state_.pos_lon = minmea_tocoord(&rmc.longitude);
      gps_state_.position_valid = true;

      double speed = minmea_tofloat(&rmc.speed) * 0.514444;  // Convert knots to m/s
      double angle_rad = minmea_tofloat(&rmc.course) * M_PI / 180.0;
      gps_state_.vel_e = sin(angle_rad) * speed;
      gps_state_.vel_n = cos(angle_rad) * speed;
      gps_state_.vel_u = 0;
      gps_state_.motion_heading_valid = true;
      gps_state_.vehicle_heading_valid = false;

      TriggerStateCallback();
      return true;
    }

    case MINMEA_SENTENCE_GSA: {
      struct minmea_sentence_gsa gsa;
      if (!minmea_parse_gsa(&gsa, line)) {
        return false;
      }

      switch (gsa.fix_type) {
        case MINMEA_GPGSA_FIX_2D: gps_state_.fix_type = GpsState::FIX_2D; break;
        case MINMEA_GPGSA_FIX_3D: gps_state_.fix_type = GpsState::FIX_3D; break;
        default: gps_state_.fix_type = GpsState::NO_FIX; break;
      }

      UpdateGpsStateValidity();
      TriggerStateCallback();
      return true;
    }

    case MINMEA_SENTENCE_GST: {
      struct minmea_sentence_gst gst;
      if (!minmea_parse_gst(&gst, line)) {
        return false;
      }

      float lat_std = minmea_tofloat(&gst.latitude_error_deviation);
      float lon_std = minmea_tofloat(&gst.longitude_error_deviation);
      float alt_std = minmea_tofloat(&gst.altitude_error_deviation);

      gps_state_.position_h_accuracy = sqrt(lat_std * lat_std + lon_std * lon_std);
      gps_state_.position_v_accuracy = alt_std;

      TriggerStateCallback();
      return true;
    }

    case MINMEA_INVALID:
      // Wrong line syntax.
      return false;

    default:
      // Correct syntax, but we're not interested in this type.
      return true;
  }
}

void NmeaGpsDriver::ResetParserState() {
  line_len = 0;
}

}  // namespace xbot::driver::gps
