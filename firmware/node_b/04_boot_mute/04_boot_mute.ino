#include <Arduino.h>

#include "ActiveBuzzer.h"
#include "DebouncedButton.h"
#include "NodeBPins.h"

namespace {
aether::ActiveBuzzer buzzer(PIN_BUZZER);
aether::DebouncedButton muteButton(PIN_BTN_MUTE);
constexpr uint32_t kStatusPeriodMs = 1000;
constexpr uint8_t kMaxSerialBytesPerLoop = 16;
uint32_t lastStatusMs = 0;
uint32_t pressCount = 0;

void printStatus(uint32_t nowMs, const char* event = "STATUS") {
  Serial.printf("{\"system\":\"AETHER\",\"node\":\"nodeB\",\"component\":\"BOOT_MUTE\","
                "\"uptime_ms\":%lu,\"event\":\"%s\",\"button_pressed\":%s,"
                "\"button_armed\":%s,\"press_count\":%lu,\"pattern\":\"%s\","
                "\"muted\":%s,\"output_high\":%s}\n",
                static_cast<unsigned long>(nowMs), event,
                muteButton.pressed() ? "true" : "false",
                muteButton.armed() ? "true" : "false",
                static_cast<unsigned long>(pressCount),
                aether::ActiveBuzzer::patternName(buzzer.pattern()),
                buzzer.muted() ? "true" : "false", buzzer.outputHigh() ? "true" : "false");
}

void serviceButton(uint32_t nowMs) {
  if (muteButton.update(nowMs)) {
    ++pressCount;
    buzzer.setMuted(true, nowMs);  // Mute idempoten; tidak melakukan toggle.
    printStatus(nowMs, "BUTTON_MUTE");
  }
}

void printHelp() {
  Serial.println("BOOT=mute; serial: t=test, w=warning, c=critical, s=stop, m=mute, u=unmute, i=status, ?=help");
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
  printStatus(nowMs, "SERIAL_COMMAND");
}
}  // namespace

void setup() {
  buzzer.begin();
  muteButton.begin(millis());
  Serial.begin(115200);
  Serial.println("AETHER Node B | Bagian 04: BOOT Mute | GPIO=0 ACTIVE=LOW DEBOUNCE=30ms");
  printHelp();
  lastStatusMs = millis();
  printStatus(lastStatusMs);
}

void loop() {
  uint32_t nowMs = millis();
  serviceButton(nowMs);  // Proses mute sebelum memperbarui fase buzzer.
  buzzer.update(nowMs);
  for (uint8_t i = 0; i < kMaxSerialBytesPerLoop && Serial.available() > 0; ++i) {
    nowMs = millis();
    serviceButton(nowMs);
    buzzer.update(nowMs);
    handleCommand(static_cast<char>(Serial.read()), nowMs);
  }
  nowMs = millis();
  serviceButton(nowMs);
  buzzer.update(nowMs);
  if (static_cast<uint32_t>(nowMs - lastStatusMs) >= kStatusPeriodMs) {
    lastStatusMs = nowMs;
    printStatus(nowMs);
  }
  delay(1);  // Yield scheduler; debounce dan pola tidak memakai delay penantian.
}

