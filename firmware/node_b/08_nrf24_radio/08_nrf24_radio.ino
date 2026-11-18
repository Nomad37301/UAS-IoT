#include <Arduino.h>
#include <RF24.h>
#include <SPI.h>

#include "AetherRfPacket.h"
#include "NodeBPins.h"
#include "NodeBRadio.h"

namespace {
constexpr uint32_t kTelemetryPeriodMs = 2000;
constexpr uint8_t kPayloadSize = sizeof(aether::AetherRfPacket);
RF24 chip(PIN_NRF_CE, PIN_SPI_CSN, 4000000);
aether::NodeBRadio radio(chip);
aether::CommandSequence commands;
aether::AetherState demoState = aether::STATE_NETWORK_DEAD;
bool autoTelemetry = false;
uint32_t lastTelemetryMs = 0;
uint16_t nextSeq = 0;
uint16_t inFlightSeq = 0;
uint32_t txAcked = 0, txFailed = 0, rxCommands = 0, rxInvalid = 0, rxDuplicates = 0;

void printHelp() {
  Serial.println("Commands: t=send test telemetry, a=toggle 2s test telemetry, 0..4=test state, r=reinit radio, i=status, ?=help");
}

void printStatus() {
  Serial.printf("{\"system\":\"AETHER\",\"node\":\"nodeB\",\"component\":\"NRF24_RADIO\","
                "\"initialized\":%s,\"busy\":%s,\"payload_bytes\":32,\"test_mode\":true,"
                "\"auto_telemetry\":%s,\"test_state\":%u,\"next_seq\":%u,"
                "\"tx_acked\":%lu,\"tx_failed\":%lu,\"rx_commands\":%lu,"
                "\"rx_invalid\":%lu,\"rx_duplicates\":%lu}\n",
                radio.ready() ? "true" : "false", radio.busy() ? "true" : "false",
                autoTelemetry ? "true" : "false", static_cast<unsigned>(demoState),
                static_cast<unsigned>(nextSeq), static_cast<unsigned long>(txAcked),
                static_cast<unsigned long>(txFailed), static_cast<unsigned long>(rxCommands),
                static_cast<unsigned long>(rxInvalid), static_cast<unsigned long>(rxDuplicates));
}

void sendTelemetry(uint32_t nowMs) {
  const auto packet = aether::makeTelemetry(nextSeq, demoState);
  const auto result = radio.send(&packet, sizeof(packet), nowMs);
  Serial.printf("{\"event\":\"TELEMETRY_TX\",\"test_mode\":true,\"seq\":%u,\"result\":\"%s\"}\n",
                static_cast<unsigned>(nextSeq), aether::NodeBRadio::sendName(result));
  if (result == aether::RadioSend::Accepted) {
    inFlightSeq = nextSeq;
    nextSeq = static_cast<uint16_t>(nextSeq + 1U);
  }
}

void serviceRadio(uint32_t nowMs) {
  radio.poll(nowMs);
  aether::RadioTx result;
  if (radio.takeTxResult(result)) {
    if (result == aether::RadioTx::Acked) ++txAcked;
    else ++txFailed;
    Serial.printf("{\"event\":\"TX_RESULT\",\"seq\":%u,\"result\":\"%s\"}\n",
                  static_cast<unsigned>(inFlightSeq), aether::NodeBRadio::txName(result));
  }
  for (uint8_t i = 0; i < 3; ++i) {
    aether::AetherRfPacket packet = {};
    if (!radio.receive(&packet, sizeof(packet))) break;
    const auto check = aether::validateCommand(packet);
    if (check != aether::CommandCheck::Valid) {
      ++rxInvalid;
      Serial.printf("{\"event\":\"RX_REJECTED\",\"reason\":\"%s\"}\n",
                    aether::commandCheckName(check));
      continue;
    }
    if (!commands.accept(packet.seq)) {
      ++rxDuplicates;
      Serial.printf("{\"event\":\"RX_DUPLICATE\",\"seq\":%u}\n", static_cast<unsigned>(packet.seq));
      continue;
    }
    ++rxCommands;
    // Tahap uji komponen: validasi dan laporkan. Aktuator digabungkan saat integrasi.
    Serial.printf("{\"event\":\"RADIO_COMMAND\",\"seq\":%u,\"action\":\"%s\",\"executed\":false}\n",
                  static_cast<unsigned>(packet.seq),
                  packet.cmd == aether::kCmdRecovery ? "TRIGGER_RECOVERY" : "MUTE_ALARM");
  }
}

void handleCommand(char command, uint32_t nowMs) {
  if (command >= '0' && command <= '4') {
    demoState = static_cast<aether::AetherState>(command - '0');
    printStatus();
    return;
  }
  switch (command) {
    case 't': sendTelemetry(nowMs); break;
    case 'a':
      autoTelemetry = !autoTelemetry;
      lastTelemetryMs = nowMs;
      printStatus();
      break;
    case 'r':
      if (radio.busy()) Serial.println("{\"event\":\"REINIT\",\"result\":\"BUSY\"}");
      else {
        const bool ok = radio.begin(SPI, kPayloadSize);
        Serial.printf("{\"event\":\"REINIT\",\"result\":\"%s\"}\n", ok ? "OK" : "FAILED");
      }
      break;
    case 'i': printStatus(); break;
    case '?': printHelp(); break;
    case '\r': case '\n': case ' ': case '\t': break;
    default: Serial.println("{\"error\":\"UNKNOWN_COMMAND\"}"); break;
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SPI_CSN);
  const bool ok = radio.begin(SPI, kPayloadSize);
  Serial.println("AETHER Node B | Bagian 08: nRF24L01+ | CH=76 RATE=250kbps PA=LOW PAYLOAD=32");
  if (!ok) Serial.println("{\"error\":\"RADIO_INIT_FAILED\"}");
  printHelp();
  printStatus();
}

void loop() {
  serviceRadio(millis());
  for (uint8_t i = 0; i < 16 && Serial.available() > 0; ++i) {
    const uint32_t nowMs = millis();
    serviceRadio(nowMs);
    handleCommand(static_cast<char>(Serial.read()), nowMs);
  }
  const uint32_t nowMs = millis();
  if (autoTelemetry && !radio.busy() && static_cast<uint32_t>(nowMs - lastTelemetryMs) >= kTelemetryPeriodMs) {
    lastTelemetryMs = nowMs;
    sendTelemetry(nowMs);
  }
  delay(1);
}
