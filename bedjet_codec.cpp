#include "bedjet_codec.h"
#include <cstdio>
#include <cstring>

namespace esphome {
namespace bedjet {

/// Log the contents of a variable length packet as hex bytes
void logVVHexPacket(const char* label, const uint8_t* buf, size_t length) {
    char output[(length * 3) + 1];

    size_t offset = 0;
    for (size_t i = 0; i < length; ++i) {
        offset += sprintf(output + offset, "%02X ", buf[i]);
    }

	ESP_LOGVV(TAG, "%s (%d bytes): %s", label, length, output);
}

/// Converts a BedJet temp step into degrees Fahrenheit.
float bedjet_temp_to_f(const uint8_t temp) {
  // BedJet temp is "C*2"; to get F, multiply by 0.9 (half 1.8) and add 32.
  return 0.9f * temp + 32.0f;
}

/** Cleans up the packet before sending. */
BedjetPacket *BedjetCodec::clean_packet_() {
  // So far no commands require more than 2 bytes of data
  if (this->packet_.data_length > 2) {
    ESP_LOGW(TAG, "Packet may be malformed");
  }
  for (int i = this->packet_.data_length; i < 2; i++) {
    this->packet_.data[i] = '\0';
  }
  ESP_LOGV(TAG, "Created packet: %02X, %02X %02X", this->packet_.command, this->packet_.data[0], this->packet_.data[1]);
  return &this->packet_;
}

/** Returns a BedjetPacket that will initiate a BedjetButton press. */
BedjetPacket *BedjetCodec::get_button_request(BedjetButton button) {
  this->packet_.command = CMD_BUTTON;
  this->packet_.data_length = 1;
  this->packet_.data[0] = button;
  return this->clean_packet_();
}

/** Returns a BedjetPacket that will set the device's target `temperature`. */
BedjetPacket *BedjetCodec::get_set_target_temp_request(float temperature) {
  this->packet_.command = CMD_SET_TEMP;
  this->packet_.data_length = 1;
  this->packet_.data[0] = temperature * 2;
  return this->clean_packet_();
}

/** Returns a BedjetPacket that will set the device's target fan speed. */
BedjetPacket *BedjetCodec::get_set_fan_speed_request(const uint8_t fan_step) {
  this->packet_.command = CMD_SET_FAN;
  this->packet_.data_length = 1;
  this->packet_.data[0] = fan_step;
  return this->clean_packet_();
}

/** Returns a BedjetPacket that will set the device's current time. */
BedjetPacket *BedjetCodec::get_set_time_request(const uint8_t hour, const uint8_t minute) {
  this->packet_.command = CMD_SET_CLOCK;
  this->packet_.data_length = 2;
  this->packet_.data[0] = hour;
  this->packet_.data[1] = minute;
  return this->clean_packet_();
}

/** Returns a BedjetPacket that will set the device's remaining runtime. */
BedjetPacket *BedjetCodec::get_set_runtime_remaining_request(const uint8_t hour, const uint8_t minute) {
  this->packet_.command = CMD_SET_RUNTIME;
  this->packet_.data_length = 2;
  this->packet_.data[0] = hour;
  this->packet_.data[1] = minute;
  return this->clean_packet_();
}

/** Decodes the extra bytes that were received after being notified with a partial packet. */
void BedjetCodec::decode_extra(const uint8_t *data, uint16_t length) {
  logVVHexPacket("Read extra bytes", data, length);
  uint8_t offset = this->last_buffer_size_;
  if (offset > 0 && length + offset <= sizeof(BedjetStatusPacket)) {
    memcpy(((uint8_t *) (&this->buf_)) + offset, data, length);
  } else {
    ESP_LOGI(TAG, "Could not determine where to append to, last offset=%d, max size=%u, new size would be %d", offset,
             sizeof(BedjetStatusPacket), length + offset);
  }
}

/** Decodes the incoming status packet received on the BEDJET_STATUS_UUID.
 *
 * @return `true` if the packet was decoded and represents a "partial" packet; `false` otherwise.
 */
bool BedjetCodec::decode_notify(const uint8_t *data, uint16_t length) {
  ESP_LOGV(TAG, "Received: %d bytes: %d %d %d %d", length, data[0], data[1], data[2], data[3]);

  if (data[1] == PACKET_FORMAT_V3_HOME && data[3] == PACKET_TYPE_STATUS) {
    logDHexPacket("Received STATUS packet", data, length);
    // Clear old buffer
    memset(&this->buf_, 0, sizeof(BedjetStatusPacket));
    // Copy new data into buffer
    memcpy(&this->buf_, data, length);
    this->last_buffer_size_ = length;

    // TODO: validate the packet checksum?
    if (this->buf_.mode < 7 && this->buf_.target_temp_step >= 38 && this->buf_.target_temp_step <= 86 &&
        this->buf_.actual_temp_step > 1 && this->buf_.actual_temp_step <= 100 && this->buf_.ambient_temp_step > 1 &&
        this->buf_.ambient_temp_step <= 100) {
      // and save it for the update() loop
      this->status_packet_ = &this->buf_;
      return this->buf_.is_partial;
    } else {
      this->status_packet_ = nullptr;
      // TODO: log a warning if we detect that we connected to a non-V3 device.
      ESP_LOGW(TAG, "Received potentially invalid packet (len %d):", length);
    }
  } else if (data[1] == PACKET_FORMAT_DEBUG || data[3] == PACKET_TYPE_DEBUG) {
    // We don't actually know the packet format for this. Dump packets to log, in case a pattern presents itself.
    logVVHexPacket("Received DEBUG packet", data, length);
  } else {
    // TODO: log a warning if we detect that we connected to a non-V3 device.
    logDHexPacket("Received UNKNOWN packet", data, length);
  }

  return false;
}

/** @return `true` if the new packet is meaningfully different from the last seen packet. */
bool BedjetCodec::compare(const uint8_t *data, uint16_t length) {
  if (data == nullptr) {
    return false;
  }

  if (length < 17) {
    // New packet looks small, skip it.
    return false;
  }

  if (this->buf_.packet_format != PACKET_FORMAT_V3_HOME ||
      this->buf_.packet_type != PACKET_TYPE_STATUS) {  // No last seen packet, so take the new one.
    return true;
  }

  if (data[1] != PACKET_FORMAT_V3_HOME || data[3] != PACKET_TYPE_STATUS) {  // New packet is not a v3 status, skip it.
    return false;
  }

  // Now coerce it to a status packet and compare some key fields
  const BedjetStatusPacket *test = reinterpret_cast<const BedjetStatusPacket *>(data);
  // These are fields that will only change due to explicit action.
  // That is why we do not check ambient or actual temp here, because those are environmental.
  bool explicit_fields_changed = this->buf_.mode != test->mode || this->buf_.fan_step != test->fan_step ||
                                 this->buf_.target_temp_step != test->target_temp_step;

  return explicit_fields_changed;
}

/// Converts a BedJet temp step into degrees Celsius.
float bedjet_temp_to_c(uint8_t temp) {
  // BedJet temp is "C*2"; to get C, divide by 2.
  return temp / 2.0f;
}

}  // namespace bedjet
}  // namespace esphome
