#pragma once

#include "AetherState.h"
#include <stddef.h>
#include <limits>
#include <math.h>

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "Kontrak biner AETHER memerlukan little-endian seperti ESP32 B/C."
#endif

namespace aether {

constexpr uint8_t kRfMagic = 0xAE;
constexpr uint8_t kRfTelemetry = 1;
constexpr uint8_t kRfCommand = 2;
constexpr uint8_t kCmdRecovery = 1;
constexpr uint8_t kCmdMute = 2;
constexpr uint8_t kFlagHttpOk = 1U << 0;
constexpr uint8_t kFlagModbusOk = 1U << 1;
constexpr uint8_t kFlagWifiOk = 1U << 2;
constexpr uint8_t kFlagRelayActive = 1U << 3;

struct __attribute__((packed)) AetherRfPacket {
  uint8_t magic;
  uint8_t msg_type;
  uint8_t state;
  uint8_t flags;
  float voltage_v;
  float ir_obj_c;
  float ir_amb_c;
  float cpu_temp_c;
  uint16_t http_code;
  uint16_t seq;
  uint8_t cmd;
  uint8_t reserved[7];  // Disetujui pengguna: 32 byte, offset field lain tetap.
};

static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
              "Radio AETHER memerlukan float IEEE-754 32 bit.");
static_assert(sizeof(AetherRfPacket) == 32, "Payload radio harus 32 byte.");
static_assert(offsetof(AetherRfPacket, voltage_v) == 4 &&
              offsetof(AetherRfPacket, ir_obj_c) == 8 &&
              offsetof(AetherRfPacket, ir_amb_c) == 12 &&
              offsetof(AetherRfPacket, cpu_temp_c) == 16 &&
              offsetof(AetherRfPacket, http_code) == 20 &&
              offsetof(AetherRfPacket, seq) == 22 &&
              offsetof(AetherRfPacket, cmd) == 24 &&
              offsetof(AetherRfPacket, reserved) == 25, "Offset kontrak berubah.");

// Data belum tersedia: NaN pada sensor, flag false, HTTP 0. Tidak mengarang metrik.
inline AetherRfPacket makeTelemetry(uint16_t seq, AetherState state) {
  AetherRfPacket packet = {};
  packet.magic = kRfMagic;
  packet.msg_type = kRfTelemetry;
  packet.state = state;
  packet.seq = seq;
  packet.voltage_v = packet.ir_obj_c = packet.ir_amb_c = packet.cpu_temp_c = NAN;
  return packet;
}

enum class CommandCheck : uint8_t { Valid, Magic, MessageType, State, Flags, Command };

inline CommandCheck validateCommand(const AetherRfPacket& packet) {
  if (packet.magic != kRfMagic) return CommandCheck::Magic;
  if (packet.msg_type != kRfCommand) return CommandCheck::MessageType;
  if (packet.state > STATE_HW_ANOMALY) return CommandCheck::State;
  if ((packet.flags & 0xF0U) != 0) return CommandCheck::Flags;
  if (packet.cmd != kCmdRecovery && packet.cmd != kCmdMute) return CommandCheck::Command;
  return CommandCheck::Valid;
}

inline const char* commandCheckName(CommandCheck check) {
  switch (check) {
    case CommandCheck::Valid: return "VALID";
    case CommandCheck::Magic: return "BAD_MAGIC";
    case CommandCheck::MessageType: return "BAD_MESSAGE_TYPE";
    case CommandCheck::State: return "BAD_STATE";
    case CommandCheck::Flags: return "BAD_FLAGS";
    case CommandCheck::Command: return "BAD_COMMAND";
  }
  return "UNKNOWN";
}

// Menandai pengulangan seq terakhir; bukan pengurutan/anti-replay lintas sesi.
class CommandSequence {
 public:
  bool accept(uint16_t seq) {
    if (seen_ && seq == last_) return false;
    seen_ = true;
    last_ = seq;
    return true;
  }
 private:
  bool seen_ = false;
  uint16_t last_ = 0;
};

}  // namespace aether

