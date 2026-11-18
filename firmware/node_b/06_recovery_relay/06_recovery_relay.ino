#include <Arduino.h>

#include "NodeBPins.h"
#include "RecoveryRelay.h"

namespace {
// Dikonfirmasi pengguna: modul relay proyek aktif HIGH.
constexpr aether::RelayPolarity kRelayPolarity = aether::RelayPolarity::ActiveHigh;
constexpr uint32_t kStatusPeriodMs = 1000;
constexpr uint8_t kMaxSerialBytesPerLoop = 16;
aether::RecoveryRelay relay(PIN_RELAY_IN, kRelayPolarity);
uint32_t lastStatusMs = 0;
uint32_t acceptedCount = 0;

void printHelp() {
  Serial.println("Commands: r=recovery pulse 500ms, s=cancel pulse, i=status, ?=help; cooldown=30s after release");
}

void printStatus(uint32_t nowMs, const char* event = "STATUS") {
  Serial.printf("{\"system\":\"AETHER\",\"node\":\"nodeB\",\"component\":\"RECOVERY_RELAY\","
                "\"uptime_ms\":%lu,\"event\":\"%s\",\"active_level\":\"HIGH\","
                "\"relay_active\":%s,\"cooldown_remaining_ms\":%lu,\"accepted_count\":%lu}\n",
                static_cast<unsigned long>(nowMs), event, relay.active() ? "true" : "false",
                static_cast<unsigned long>(relay.cooldownRemainingMs(nowMs)),
                static_cast<unsigned long>(acceptedCount));
}

void serviceRelay(uint32_t nowMs) {
  const bool wasActive = relay.active();
  relay.update(nowMs);
  if (wasActive && !relay.active()) printStatus(nowMs, "PULSE_RELEASED");
}

void handleCommand(char command, uint32_t nowMs) {
  switch (command) {
    case 'r': {
      const aether::RelayTriggerResult result = relay.trigger(nowMs);
      if (result == aether::RelayTriggerResult::Started) ++acceptedCount;
      printStatus(nowMs, aether::RecoveryRelay::resultName(result));
      return;
    }
    case 's': relay.cancel(nowMs); printStatus(nowMs, "CANCEL_REQUEST"); return;
    case 'i': printStatus(nowMs); return;
    case '?': printHelp(); return;
    case '\r': case '\n': case ' ': case '\t': return;
    default: Serial.println("{\"error\":\"UNKNOWN_COMMAND\",\"help\":\"?\"}"); return;
  }
}
}  // namespace

void setup() {
  relay.begin();  // Aktif HIGH: perintahkan output LOW, tidak ada recovery otomatis.
  Serial.begin(115200);
  Serial.println("AETHER Node B | Bagian 06: Recovery Relay | GPIO=16 ACTIVE=HIGH");
  printHelp();
  lastStatusMs = millis();
  printStatus(lastStatusMs);
}

void loop() {
  uint32_t nowMs = millis();
  serviceRelay(nowMs);
  for (uint8_t i = 0; i < kMaxSerialBytesPerLoop && Serial.available() > 0; ++i) {
    nowMs = millis();
    serviceRelay(nowMs);
    handleCommand(static_cast<char>(Serial.read()), nowMs);
  }
  nowMs = millis();
  serviceRelay(nowMs);
  if (static_cast<uint32_t>(nowMs - lastStatusMs) >= kStatusPeriodMs) {
    lastStatusMs = nowMs;
    printStatus(nowMs);
  }
  delay(1);
}

