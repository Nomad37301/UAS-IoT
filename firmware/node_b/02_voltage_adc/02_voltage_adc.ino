#include <Arduino.h>

#include "NodeBPins.h"
#include "VoltageSensor.h"

namespace {
constexpr uint32_t kReportPeriodMs = 1000;
// Default mengikuti rumus master persis. Ubah gain setelah ukur multimeter.
constexpr float kVoltageGain = 1.0f;
constexpr float kVoltageOffsetV = 0.0f;

aether::VoltageSensor sensor(PIN_VOLT_ADC, kVoltageGain, kVoltageOffsetV);
uint32_t lastReportMs = 0;
uint32_t sequence = 0;
}  // namespace

void setup() {
  Serial.begin(115200);
  sensor.begin();
  Serial.println("AETHER Node B | Bagian 02: Voltage ADC | GPIO=4 ADC1_CH3 RATIO=5");
  lastReportMs = millis();
}

void loop() {
  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - lastReportMs) < kReportPeriodMs) {
    delay(1);
    return;
  }
  lastReportMs = now;

  aether::VoltageReading reading;
  const aether::VoltageError error = sensor.read(reading);
  ++sequence;
  Serial.printf("{\"system\":\"AETHER\",\"node\":\"nodeB\",\"component\":\"VOLTAGE_ADC\","
                "\"seq\":%lu,\"uptime_ms\":%lu,\"valid\":%s,",
                static_cast<unsigned long>(sequence), static_cast<unsigned long>(now),
                reading.valid ? "true" : "false");
  if (reading.valid) {
    Serial.printf("\"adc_mv\":%.2f,\"voltage_v\":%.3f,\"low_voltage\":%s,",
                  reading.adc_mv, reading.voltage_v, reading.low_voltage ? "true" : "false");
  } else {
    Serial.print("\"adc_mv\":null,\"voltage_v\":null,\"low_voltage\":null,");
  }
  Serial.printf("\"error\":\"%s\"}\n", aether::VoltageSensor::errorName(error));
}

