#include <Arduino.h>
#include <Wire.h>

#include "Mlx90614.h"
#include "NodeBPins.h"

namespace {
constexpr uint32_t kSamplePeriodMs = 1000;
constexpr uint32_t kStartupWaitMs = 1000;
constexpr uint16_t kI2cTimeoutMs = 50;

aether::Mlx90614 sensor(Wire, MLX90614_I2C_ADDR);
bool busReady = false;
uint32_t lastSampleMs = 0;
uint32_t sequence = 0;

bool startBus() {
  if (!Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000)) return false;
  Wire.setTimeOut(kI2cTimeoutMs);
  return true;
}
}  // namespace

void setup() {
  Serial.begin(115200);  // Port USB UART pada gambar board Node B.
  busReady = startBus();
  delay(kStartupWaitMs);  // Satu kali saat boot; beri waktu hasil sensor stabil.
  Serial.println("AETHER Node B | Bagian 01: GY-906 | SDA=8 SCL=9 ADDR=0x5A CLK=100kHz");
  lastSampleMs = millis() - kSamplePeriodMs;
}

void loop() {
  const uint32_t now = millis();
  // Selisih unsigned tetap benar ketika millis() melimpah.
  if (static_cast<uint32_t>(now - lastSampleMs) < kSamplePeriodMs) {
    delay(1);  // Beri waktu scheduler; tidak menunggu satu interval penuh.
    return;
  }
  lastSampleMs = now;
  if (!busReady) busReady = startBus();

  aether::TemperatureReading reading;
  const aether::TemperatureError error = busReady
      ? sensor.read(reading) : aether::TemperatureError::BusNotReady;
  ++sequence;

  // Field suhu siap dipakai kembali oleh telemetri AETHER pada tahap integrasi.
  // JSON menggunakan null saat gagal, bukan NaN atau suhu 0 palsu.
  Serial.printf("{\"system\":\"AETHER\",\"node\":\"nodeB\",\"component\":\"GY-906\","
                "\"seq\":%lu,\"uptime_ms\":%lu,\"valid\":%s,",
                static_cast<unsigned long>(sequence),
                static_cast<unsigned long>(now), reading.valid ? "true" : "false");
  if (reading.valid) {
    Serial.printf("\"ir_obj_c\":%.2f,\"ir_amb_c\":%.2f,", reading.ir_obj_c, reading.ir_amb_c);
  } else {
    Serial.print("\"ir_obj_c\":null,\"ir_amb_c\":null,");
  }
  Serial.printf("\"error\":\"%s\"}\n", aether::Mlx90614::errorName(error));
}

