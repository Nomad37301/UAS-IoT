#pragma once

#include <Arduino.h>
#include <math.h>

namespace aether {

enum class VoltageError : uint8_t {
  None,
  NotStarted,
  InvalidCalibration,
  AdcSaturated,
  OutOfRange,
};

struct VoltageReading {
  float voltage_v = NAN;
  float adc_mv = NAN;
  bool valid = false;
  bool low_voltage = false;
};

class VoltageSensor {
 public:
  // gain mengoreksi toleransi pembagi; offset dalam volt sisi Raspberry Pi.
  explicit VoltageSensor(uint8_t pin, float gain = 1.0f, float offset_v = 0.0f)
      : pin_(pin), gain_(gain), offset_v_(offset_v) {}

  void begin();
  VoltageError read(VoltageReading& reading);
  static const char* errorName(VoltageError error);

 private:
  uint8_t pin_;
  float gain_;
  float offset_v_;
  bool started_ = false;
};

}  // namespace aether

