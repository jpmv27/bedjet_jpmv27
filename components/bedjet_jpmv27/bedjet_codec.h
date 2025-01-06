#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "bedjet_const.h"

namespace esphome {
namespace bedjet {

struct BedjetPacket {
  uint8_t data_length;
  BedjetCommand command;
  uint8_t data[2];
};

enum BedjetPacketFormat : uint8_t {
  PACKET_FORMAT_DEBUG = 0x05,    //  5
  PACKET_FORMAT_V3_HOME = 0x56,  // 86
};

enum BedjetPacketType : uint8_t {
  PACKET_TYPE_STATUS = 0x1,
  PACKET_TYPE_DEBUG = 0x2,
};

enum BedjetNotification : uint8_t {
  NOTIFY_NONE = 0,                    ///< No notification pending
  NOTIFY_FILTER = 1,                  ///< Clean Filter / Please check BedJet air filter and clean if necessary.
  NOTIFY_UPDATE = 2,                  ///< Firmware Update / A newer version of firmware is available.
  NOTIFY_UPDATE_FAIL = 3,             ///< Firmware Update / Unable to connect to the firmware update server.
  NOTIFY_BIO_FAIL_CLOCK_NOT_SET = 4,  ///< The specified sequence cannot be run because the clock is not set
  NOTIFY_BIO_FAIL_TOO_LONG = 5,       ///< The specified sequence cannot be run because it contains steps that would be too
                                      ///< long running from the current time.
  // Note: after handling a notification, send MAGIC_NOTIFY_ACK
};

/** The format of a BedJet V3 status packet. */

// Ubertooth capture of a notification packet
//
// [Data Header].......................[BLE LINK LAYER PDU]....................................[CRC]
// flags                                                                                       |
// |  length = 27                                                                              |
// |  |   [L2CAP PDU]                                                                          |
// |  |   length = 23                                                                          |
// |  |   |  CID = 0x0004 (ATT)                                                                |
// |  |   |  |        [ATT PDU]                                                                |
// |  |   |  |        opcode = Handle Value Notification                                       |
// |  |   |  |        |  handle = 0x002a (unknown)                                             |
// |  |   |  |        |  |      [BedJet PDU]                                                   |
// |  |   |  |        |  |      is-partial = yes                                               |
// |  |   |  |        |  |      |  format = V3-home                                            |
// |  |   |  |        |  |      |  |  total length = 27                                        |
// |  |   |  |        |  |      |  |  |  type = status                                         |
// |  |   |  |        |  |      |  |  |  |  remaining HH:MM:SS                                 |
// |  |   |  |        |  |      |  |  |  |  |        actual temp = 20C                         |
// |  |   |  |        |  |      |  |  |  |  |        |  target temp = 32C                      |
// |  |   |  |        |  |      |  |  |  |  |        |  |  mode = standby                      |
// |  |   |  |        |  |      |  |  |  |  |        |  |  |  fan speed = 60%                  |
// |  |   |  |        |  |      |  |  |  |  |        |  |  |  |  max runtime HH:MM             |
// |  |   |  |        |  |      |  |  |  |  |        |  |  |  |  |     min temp = 10C          |
// |  |   |  |        |  |      |  |  |  |  |        |  |  |  |  |     |  max temp = 40C       |
// |  |   |  |        |  |      |  |  |  |  |        |  |  |  |  |     |  |  turbo time (HH:MM?)
// |  |   |  |        |  |      |  |  |  |  |        |  |  |  |  |     |  |  |     ambient temp = 20C
// |  |   |  |        |  |      |  |  |  |  |        |  |  |  |  |     |  |  |     |  shutdown reason
// |  |   |  |        |  |      |  |  |  |  |        |  |  |  |  |     |  |  |     |  |  ?     |
// |  |   |  |        |  |      |  |  |  |  |        |  |  |  |  |     |  |  |     |  |  |     |
// V  V   V  V        V  V      V  V  V  V  V        V  V  V  V  V     V  V  V     V  V  V     V
// 06 1b {17 00 04 00 1b 2a 00 [01 56 1b 01 00 00 00 28 40 00 0b 00 00 14 50 00 00 28 00 12]} {db c4 95}

struct BedjetStatusPacket {
  // [ 0]
  bool is_partial : 8;                      ///< `1` indicates that this is a partial packet, and more data can be read directly from the
                                            ///< characteristic.
  // [ 1]
  BedjetPacketFormat packet_format : 8;     ///< BedjetPacketFormat::PACKET_FORMAT_V3_HOME for BedJet V3 status packet
                                            ///< format. BedjetPacketFormat::PACKET_FORMAT_DEBUG for debugging packets.

  // [ 2]
  uint8_t expecting_length : 8;             ///< The expected total length of the status packet after merging the extra packet.
                                            ///< (mine says 27). Perhaps the first four bytes are considered a packet header and
                                            ///< not included in the length

  // [ 3]
  BedjetPacketType packet_type : 8;         ///< Typically BedjetPacketType::PACKET_TYPE_STATUS for BedJet V3 status packet.

  // [ 4]
  uint8_t time_remaining_hrs : 8;           ///< Hours remaining in program runtime

  // [ 5]
  uint8_t time_remaining_mins : 8;          ///< Minutes remaining in program runtime

  // [ 6]
  uint8_t time_remaining_secs : 8;          ///< Seconds remaining in program runtime

  // [ 7]
  uint8_t actual_temp_step : 8;             ///< Actual temp of the air blown by the BedJet fan; value represents `2 *
                                            ///< degrees_celsius`. See #bedjet_temp_to_c and #bedjet_temp_to_f

  // [ 8]
  uint8_t target_temp_step : 8;             ///< Target temp that the BedJet will try to heat to. See #actual_temp_step.

  // [ 9]
  BedjetMode mode : 8;                      ///< BedJet operating mode. See enum BedjetMode

  // [10]
  uint8_t fan_step : 8;                     ///< BedJet fan speed; value is in the 0-19 range, representing 5% increments (5%-100%): `5 + 5
                                            ///< * fan_step`

  // [11]
  uint8_t max_hrs : 8;                      ///< Max hours of mode runtime

  // [12]
  uint8_t max_mins : 8;                     ///< Max minutes of mode runtime

  // [13]
  uint8_t min_temp_step : 8;                ///< Min temp allowed in mode. See #actual_temp_step.

  // [14]
  uint8_t max_temp_step : 8;                ///< Max temp allowed in mode. See #actual_temp_step.

  // [15-16]
  uint16_t turbo_time : 16;                 ///< Time remaining in BedjetMode::MODE_TURBO.

  // [17]
  uint8_t ambient_temp_step : 8;            ///< Current ambient air temp. This is the coldest air the BedJet can blow. See
                                            ///< #actual_temp_step.

  // [18]
  uint8_t shutdown_reason : 8;              ///< The reason for the last device shutdown.

  // Something is not right after this point. My notification packet has an extra byte,
  // and my read packet starts with an 0x01. But my expected length is 27 and the total
  // size of notification packet and read packet is 20 + 11 = 31. Looks like the original
  // author make an off-by-one error in this area as well.

  // [19]
  uint8_t unknown_1 : 8;                    // Unknown = 0x01 (0x12 in mine)

  // *** The notification partial packet cuts off here after [19] ***

  // [20]
  uint8_t unknown_2 : 8;                    // Unknown = 0x81 (0x01 in mine)

  // [21]
  uint8_t unknown_3 : 8;                    // Unknown = 0x01 (0x9A in mine)

  // [22]                                   // (mine: 0x01=off, 0xF1=bio, 0x81=other
  struct {
    int unknown_1 : 1;       // 0x80
    int unknown_2 : 1;       // 0x40
    int unknown_3 : 1;       // 0x20
    int unknown_4 : 1;       // 0x10
    int unknown_5 : 1;       // 0x08
    int unknown_6 : 1;       // 0x04
    bool is_dual_zone : 1;   // 0x02        /// Is part of a Dual Zone configuration
    int unknown_7 : 1;       // 0x01
  } dual_zone_flags;                            // NOLINT(clang-diagnostic-unaligned-access)

  // [23]
  uint8_t unknown_4 : 8;                    // Unknown = 0x10

  // [24]
  uint8_t unknown_5 : 8;                    // Unknown = 0x12

  // [25]
  uint8_t unknown_6 : 8;                    // Unknown = 0x00

  // [26]
  uint8_t update_phase : 8;                 ///< The current status/phase of a firmware update.
                                            ///<  0x14(20) = ??? (mine)
                                            ///<  0x18(24) = "Connection test has completed OK"
                                            ///<  0x1a(26) = "Firmware update is not needed"

  // [27]
  union {
    uint8_t flags_packed;
    struct {
      /* uint8_t */
      int unknown_1 : 1;           // 0x80
      int unknown_2 : 1;           // 0x40
      bool conn_test_passed : 1;   // 0x20  ///< Bit is set `1` if the last connection test passed.
      bool leds_enabled : 1;       // 0x10  ///< Bit is set `1` if the LEDs on the device are enabled.
      int unknown_3 : 1;           // 0x08
      bool units_setup : 1;        // 0x04  ///< Bit is set `1` if the device's units have been configured.
      int unknown_4 : 1;           // 0x02
      bool beeps_muted : 1;        // 0x01  ///< Bit is set `1` if the device's sound output is muted.
    } __attribute__((packed)) flags;
  };

  // [28]
  uint8_t bio_sequence_step : 8;            /// Biorhythm sequence step number

  // [29]
  BedjetNotification notify_code : 8;       /// See BedjetNotification

  // [30]
  uint16_t unknown_7 : 8;                   // Unknown (mine varies, counts up/down)

} __attribute__((packed));

/** This class is responsible for encoding command packets and decoding status packets.
 *
 * Status Packets
 * ==============
 * The BedJet protocol depends on registering for notifications on the esphome::BedJet::BEDJET_SERVICE_UUID
 * characteristic. If the BedJet is on, it will send rapid updates as notifications. If it is off,
 * it generally will not notify of any status.
 *
 * As the BedJet V3's BedjetStatusPacket exceeds the buffer size allowed for BLE notification packets,
 * the notification packet will contain `BedjetStatusPacket::is_partial == 1`. When that happens, an additional
 * read of the esphome::BedJet::BEDJET_SERVICE_UUID characteristic will contain the second portion of the
 * full status packet.
 *
 * Command Packets
 * ===============
 * This class supports encoding a number of BedjetPacket commands:
 * - Button press
 *   This simulates a press of one of the BedjetButton values.
 *   - BedjetPacket#command = BedjetCommand::CMD_BUTTON
 *   - BedjetPacket#data [0] contains the BedjetButton value
 * - Set target temp
 *   This sets the BedJet's target temp to a concrete temperature value.
 *   - BedjetPacket#command = BedjetCommand::CMD_SET_TEMP
 *   - BedjetPacket#data [0] contains the BedJet temp value; see BedjetStatusPacket#actual_temp_step
 * - Set fan speed
 *   This sets the BedJet fan speed.
 *   - BedjetPacket#command = BedjetCommand::CMD_SET_FAN
 *   - BedjetPacket#data [0] contains the BedJet fan step in the range 0-19.
 * - Set current time
 *   The BedJet needs to have its clock set properly in order to run the biorhythm programs, which might
 *   contain time-of-day based step rules.
 *   - BedjetPacket#command = BedjetCommand::CMD_SET_CLOCK
 *   - BedjetPacket#data [0] is hours, [1] is minutes
 */
class BedjetCodec {
 public:
  BedjetPacket *get_button_request(BedjetButton button);
  BedjetPacket *get_set_target_temp_request(float temperature);
  BedjetPacket *get_set_fan_speed_request(uint8_t fan_step);
  BedjetPacket *get_set_time_request(uint8_t hour, uint8_t minute);
  BedjetPacket *get_set_runtime_remaining_request(uint8_t hour, uint8_t minute);

  bool decode_notify(const uint8_t *data, uint16_t length);
  void decode_extra(const uint8_t *data, uint16_t length);
  bool compare(const uint8_t *data, uint16_t length);

  inline bool has_status() { return this->status_packet_ != nullptr; }
  const BedjetStatusPacket *get_status_packet() const { return this->status_packet_; }
  void clear_status() { this->status_packet_ = nullptr; }

 protected:
  BedjetPacket *clean_packet_();

  uint8_t last_buffer_size_ = 0;

  BedjetPacket packet_;

  BedjetStatusPacket *status_packet_;
  BedjetStatusPacket buf_;
};

/// Converts a BedJet temp step into degrees Celsius.
float bedjet_temp_to_c(uint8_t temp);

}  // namespace bedjet
}  // namespace esphome
