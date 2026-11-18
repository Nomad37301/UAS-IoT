#include "AetherRfPacket.h"
#include "NodeBRadio.h"

#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

int main() {
  using namespace aether;
  static_assert(sizeof(AetherRfPacket) == 32, "Kontrak fixed payload");
  auto telemetry = makeTelemetry(0x1234, STATE_SERVICE_DOWN);
  assert(telemetry.magic == 0xAE && telemetry.msg_type == 1);
  assert(telemetry.seq == 0x1234 && telemetry.state == 2);
  assert(telemetry.flags == 0 && telemetry.cmd == 0 && telemetry.http_code == 0);
  assert(std::isnan(telemetry.voltage_v) && std::isnan(telemetry.ir_obj_c) &&
         std::isnan(telemetry.ir_amb_c) && std::isnan(telemetry.cpu_temp_c));
  for (uint8_t byte : telemetry.reserved) assert(byte == 0);

  telemetry.flags = kFlagHttpOk | kFlagModbusOk | kFlagWifiOk | kFlagRelayActive;
  telemetry.voltage_v = 5.0f;
  telemetry.ir_obj_c = 37.5f;
  telemetry.ir_amb_c = 25.0f;
  telemetry.cpu_temp_c = 50.0f;
  telemetry.http_code = 200;
  const uint8_t golden[32] = {
    0xAE, 1, 2, 0x0F, 0, 0, 0xA0, 0x40, 0, 0, 0x16, 0x42,
    0, 0, 0xC8, 0x41, 0, 0, 0x48, 0x42, 0xC8, 0, 0x34, 0x12,
    0, 0, 0, 0, 0, 0, 0, 0
  };
  assert(std::memcmp(&telemetry, golden, 32) == 0);

  AetherRfPacket command = {};
  command.magic = kRfMagic;
  command.msg_type = kRfCommand;
  command.cmd = kCmdRecovery;
  command.seq = 65535;
  assert(validateCommand(command) == CommandCheck::Valid);
  command.cmd = kCmdMute;
  assert(validateCommand(command) == CommandCheck::Valid);
  command.magic = 0;
  assert(validateCommand(command) == CommandCheck::Magic);
  command.magic = kRfMagic; command.msg_type = kRfTelemetry;
  assert(validateCommand(command) == CommandCheck::MessageType);
  command.msg_type = kRfCommand; command.state = 5;
  assert(validateCommand(command) == CommandCheck::State);
  command.state = STATE_HW_ANOMALY; command.flags = 0x10;
  assert(validateCommand(command) == CommandCheck::Flags);
  command.flags = 0x0F; command.cmd = 0;
  assert(validateCommand(command) == CommandCheck::Command);
  command.cmd = 3;
  assert(validateCommand(command) == CommandCheck::Command);
  command.cmd = kCmdMute;
  // Reserved diabaikan penerima, tetap nol saat factory membuat telemetry.
  std::memset(command.reserved, 0xFF, sizeof(command.reserved));
  assert(validateCommand(command) == CommandCheck::Valid);

  CommandSequence sequence;
  assert(sequence.accept(65535));
  assert(!sequence.accept(65535));
  assert(sequence.accept(0));
  assert(!sequence.accept(0));
  assert(sequence.accept(1));

  SPIClass spi;
  RF24 chip;
  NodeBRadio link(chip);
  assert(link.begin(spi, sizeof(AetherRfPacket)));
  assert(chip.payloadSize == 32);
  assert(link.send(&telemetry, sizeof(telemetry), 0) == RadioSend::Accepted);
  assert(chip.lastSent.size() == 32 && std::memcmp(chip.lastSent.data(), golden, 32) == 0);
  chip.status = RF24_TX_DS;
  link.poll(1);
  RadioTx result;
  assert(link.takeTxResult(result) && result == RadioTx::Acked);
  const auto* bytes = reinterpret_cast<const uint8_t*>(&command);
  chip.incoming.push_back({1, std::vector<uint8_t>(bytes, bytes + sizeof(command))});
  AetherRfPacket received = {};
  assert(link.receive(&received, sizeof(received)));
  assert(validateCommand(received) == CommandCheck::Valid);
  assert(received.seq == 65535 && received.cmd == kCmdMute);
  std::cout << "PASS: packed 32-byte layout, little-endian golden bytes, missing metrics, "
               "reserved[7], command validation, duplicate/rollover, 32-byte TX/RX\n";
}
