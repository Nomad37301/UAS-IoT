#include <Arduino.h>

#include "NodeBPins.h"
#include "StatusRgb.h"

namespace {
constexpr uint8_t kBrightness = 32;
constexpr uint32_t kStatusPeriodMs = 1000;
constexpr uint8_t kMaxSerialBytesPerLoop = 16;
aether::StatusRgb led(PIN_RGB_LED, kBrightness);
uint32_t lastStatusMs = 0;

void printHelp() {
  Serial.println("Commands: 0=healthy, 1=ext-isolated, 2=service-down, 3=network-dead, 4=hw-anomaly, t=RGB test, s=off, i=status, ?=help");
}

void printStatus(uint32_t nowMs) {
  Serial.printf("{\"system\":\"AETHER\",\"node\":\"nodeB\",\"component\":\"RGB_STATUS\","
                "\"uptime_ms\":%lu,\"mode\":\"%s\",\"state_code\":",
                static_cast<unsigned long>(nowMs), aether::StatusRgb::modeName(led.mode()));
  if (led.mode() == aether::RgbMode::State) {
    Serial.printf("%u,\"state\":\"%s\",", static_cast<unsigned>(led.state()),
                  aether::StatusRgb::stateName(led.state()));
  } else {
    Serial.print("null,\"state\":null,");
  }
  Serial.printf("\"red\":%u,\"green\":%u,\"blue\":%u}\n",
                static_cast<unsigned>(led.red()), static_cast<unsigned>(led.green()),
                static_cast<unsigned>(led.blue()));
}

void handleCommand(char command, uint32_t nowMs) {
  if (command >= '0' && command <= '4') {
    led.setState(static_cast<aether::AetherState>(command - '0'), nowMs);
  } else {
    switch (command) {
      case 't': led.startTest(nowMs); break;
      case 's': led.off(nowMs); break;
      case 'i': break;
      case '?': printHelp(); return;
      case '\r': case '\n': case ' ': case '\t': return;
      default:
        Serial.println("{\"error\":\"UNKNOWN_COMMAND\",\"help\":\"?\"}");
        return;
    }
  }
  printStatus(nowMs);
}
}  // namespace

void setup() {
  led.begin();
  Serial.begin(115200);
  Serial.println("AETHER Node B | Bagian 05: WS2812 RGB | GPIO=48 ORDER=GRB BRIGHTNESS=32");
  printHelp();
  lastStatusMs = millis();
  printStatus(lastStatusMs);
}

void loop() {
  led.update(millis());
  for (uint8_t i = 0; i < kMaxSerialBytesPerLoop && Serial.available() > 0; ++i) {
    const uint32_t nowMs = millis();
    led.update(nowMs);
    handleCommand(static_cast<char>(Serial.read()), nowMs);
  }
  const uint32_t nowMs = millis();
  led.update(nowMs);
  if (static_cast<uint32_t>(nowMs - lastStatusMs) >= kStatusPeriodMs) {
    lastStatusMs = nowMs;
    printStatus(nowMs);
  }
  delay(1);
}

