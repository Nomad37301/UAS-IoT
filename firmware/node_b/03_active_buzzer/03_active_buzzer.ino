#include <Arduino.h>

#include "ActiveBuzzer.h"
#include "NodeBPins.h"

namespace {
constexpr uint32_t kStatusPeriodMs = 1000;
constexpr uint8_t kMaxSerialBytesPerLoop = 16;
aether::ActiveBuzzer buzzer(PIN_BUZZER);
uint32_t lastStatusMs = 0;

void printHelp() {
  Serial.println("Commands: t=test 100ms, w=warning, c=critical, s=stop, m=mute, u=unmute, i=status, ?=help");
}

void printStatus(uint32_t nowMs) {
  Serial.printf("{\"system\":\"AETHER\",\"node\":\"nodeB\",\"component\":\"ACTIVE_BUZZER\","
                "\"uptime_ms\":%lu,\"pattern\":\"%s\",\"muted\":%s,\"output_high\":%s}\n",
                static_cast<unsigned long>(nowMs),
                aether::ActiveBuzzer::patternName(buzzer.pattern()),
                buzzer.muted() ? "true" : "false", buzzer.outputHigh() ? "true" : "false");
}

void handleCommand(char command, uint32_t nowMs) {
  switch (command) {
    case 't': buzzer.setPattern(aether::BuzzerPattern::Test, nowMs); break;
    case 'w': buzzer.setPattern(aether::BuzzerPattern::Warning, nowMs); break;
    case 'c': buzzer.setPattern(aether::BuzzerPattern::Critical, nowMs); break;
    case 's': buzzer.setPattern(aether::BuzzerPattern::Off, nowMs); break;
    case 'm': buzzer.setMuted(true, nowMs); break;
    case 'u': buzzer.setMuted(false, nowMs); break;
    case 'i': break;
    case '?': printHelp(); return;
    case '\r': case '\n': case ' ': case '\t': return;
    default:
      Serial.println("{\"error\":\"UNKNOWN_COMMAND\",\"help\":\"?\"}");
      return;
  }
  printStatus(nowMs);
}
}  // namespace

void setup() {
  buzzer.begin();  // Set output LOW sebelum memulai diagnostik serial.
  Serial.begin(115200);
  Serial.println("AETHER Node B | Bagian 03: Active Buzzer | GPIO=15 ACTIVE=HIGH");
  printHelp();
  lastStatusMs = millis();
  printStatus(lastStatusMs);
}

void loop() {
  buzzer.update(millis());
  // Batasi pekerjaan CLI agar pola tetap diperbarui saat input serial ramai.
  for (uint8_t i = 0; i < kMaxSerialBytesPerLoop && Serial.available() > 0; ++i) {
    const uint32_t nowMs = millis();
    buzzer.update(nowMs);
    handleCommand(static_cast<char>(Serial.read()), nowMs);
  }
  const uint32_t nowMs = millis();
  buzzer.update(nowMs);
  if (static_cast<uint32_t>(nowMs - lastStatusMs) >= kStatusPeriodMs) {
    lastStatusMs = nowMs;
    printStatus(nowMs);
  }
  delay(1);  // Yield ke scheduler; driver tidak memiliki delay pola.
}

