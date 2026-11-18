#include "Mlx90614.h"

#include <cassert>
#include <cmath>
#include <iostream>

using aether::Mlx90614;
using aether::TemperatureError;
using aether::TemperatureReading;

// Implementasi referensi berbasis pembagian polinomial, berbeda dari loop driver.
static uint8_t referencePec(uint8_t reg, uint16_t raw) {
  uint16_t remainder = 0;
  const uint8_t bytes[] = {0xB4, reg, 0xB5, static_cast<uint8_t>(raw),
                           static_cast<uint8_t>(raw >> 8)};
  for (uint8_t byte : bytes) {
    remainder ^= static_cast<uint16_t>(byte) << 8;
    for (int bit = 0; bit < 8; ++bit) {
      remainder = static_cast<uint16_t>((remainder << 1) ^
                                      ((remainder & 0x8000) ? 0x0700 : 0));
    }
  }
  return static_cast<uint8_t>(remainder >> 8);
}

static TwoWire::Reply reply(uint8_t reg, uint16_t raw) {
  return {reg, {static_cast<uint8_t>(raw), static_cast<uint8_t>(raw >> 8),
                referencePec(reg, raw)}, 0};
}

static void expectInvalid(const TemperatureReading& reading) {
  assert(!reading.valid);
  assert(std::isnan(reading.ir_obj_c));
  assert(std::isnan(reading.ir_amb_c));
}

int main() {
  // Vektor independen pada gambar 6 datasheet Melexis.
  assert(referencePec(0x07, 0x3AD2) == 0x30);
  TwoWire wire;
  Mlx90614 sensor(wire);
  TemperatureReading reading;

  wire.replies = {reply(0x06, 15000), {0x07, {0xD2, 0x3A, 0x30}, 0}};
  assert(sensor.read(reading) == TemperatureError::None);
  assert(reading.valid);
  assert(std::fabs(reading.ir_amb_c - 26.85f) < 0.001f);
  assert(std::fabs(reading.ir_obj_c - 28.01f) < 0.001f);
  assert(wire.replies.empty());

  // Data rusak harus membatalkan pasangan, termasuk hasil valid sebelumnya.
  wire.replies = {reply(0x06, 15000), {0x07, {0xD3, 0x3A, 0x30}, 0}};
  assert(sensor.read(reading) == TemperatureError::PecMismatch);
  expectInvalid(reading);

  wire.replies = {reply(0x06, 15000), reply(0x07, 0x8001)};
  assert(sensor.read(reading) == TemperatureError::SensorFault);
  expectInvalid(reading);

  wire.replies = {{0x06, {0xAA, 0xBB}, 0}};
  assert(sensor.read(reading) == TemperatureError::ShortRead);
  expectInvalid(reading);
  assert(wire.available() == 0);

  wire.replies = {{0x06, {}, 0}};
  assert(sensor.read(reading) == TemperatureError::ShortRead);
  expectInvalid(reading);

  for (uint8_t code : {2, 3, 4, 5}) {
    wire.replies = {{0x06, {}, code}};
    const auto expected = code == 5 ? TemperatureError::Timeout
                        : code == 4 ? TemperatureError::Bus : TemperatureError::Nack;
    assert(sensor.read(reading) == expected);
    expectInvalid(reading);
    assert(wire.replies.empty());
  }

  wire.replies = {{0x06, {}, 0}};
  wire.acceptWrite = false;
  assert(sensor.read(reading) == TemperatureError::TxBuffer);
  expectInvalid(reading);
  wire.acceptWrite = true;

  // Sampel selanjutnya pulih otomatis dan suhu negatif tetap sah.
  wire.replies = {reply(0x06, 15000), reply(0x07, 13000)};
  assert(sensor.read(reading) == TemperatureError::None);
  assert(reading.valid);
  assert(std::fabs(reading.ir_obj_c - (-13.15f)) < 0.001f);
  std::cout << "PASS: SMBus ordering, PEC vector, conversion, errors, stale invalidation, recovery\n";
}

