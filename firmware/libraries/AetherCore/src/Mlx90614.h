#pragma once

#include <Wire.h>
#include <math.h>
#include <stdint.h>

namespace aether {

enum class TemperatureError : uint8_t {
  None,
  BusNotReady,
  TxBuffer,
  Nack,
  Bus,
  Timeout,
  ShortRead,
  PecMismatch,
  SensorFault,
};

struct TemperatureReading {
  float ir_obj_c = NAN;
  float ir_amb_c = NAN;
  bool valid = false;
};

// Driver SMBus read-only: tidak mengubah EEPROM atau kalibrasi sensor.
// Pemilik bus memanggil Wire.begin() dan mengatur timeout sebelum read().
class Mlx90614 {
 public:
  explicit Mlx90614(TwoWire& wire, uint8_t address = 0x5A)
      : wire_(wire), address_(address) {}

  TemperatureError read(TemperatureReading& reading);
  static const char* errorName(TemperatureError error);

 private:
  TemperatureError readTemperature(uint8_t reg, float& temperature);
  static uint8_t crc8(const uint8_t* bytes, size_t count);

  TwoWire& wire_;
  uint8_t address_;
};

}  // namespace aether

