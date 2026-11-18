#include "VoltageSensor.h"

namespace aether {
namespace {
constexpr uint8_t kSamples = 16;
constexpr uint16_t kRawMaximum = 4095;
constexpr uint32_t kAdcUpperLimitMv = 3100;  // ADC_11db ESP32-S3.
constexpr float kDividerRatio = 5.0f;
constexpr float kLowVoltageV = 4.5f;
}  // namespace

void VoltageSensor::begin() {
  pinMode(pin_, INPUT);  // Tidak memakai pull-up/down internal pada input analog.
  analogReadResolution(12);
  analogSetPinAttenuation(pin_, ADC_11db);
  started_ = true;
}

VoltageError VoltageSensor::read(VoltageReading& reading) {
  reading = VoltageReading{};  // Jangan menyajikan sampel lama saat gagal.
  if (!started_) return VoltageError::NotStarted;
  if (!isfinite(gain_) || gain_ <= 0.0f || !isfinite(offset_v_)) {
    return VoltageError::InvalidCalibration;
  }

  uint32_t sumMv = 0;
  for (uint8_t i = 0; i < kSamples; ++i) {
    // Dua konversi terpisah: raw untuk mendeteksi clipping, mV untuk konversi
    // terkalibrasi dari core. Keduanya tidak diklaim sebagai sampel identik.
    const uint16_t raw = analogRead(pin_);
    if (raw >= kRawMaximum) return VoltageError::AdcSaturated;
    const uint32_t mv = analogReadMilliVolts(pin_);
    if (mv >= kAdcUpperLimitMv) return VoltageError::OutOfRange;
    sumMv += mv;
  }

  const float meanMv = static_cast<float>(sumMv) / kSamples;
  const float voltage = (meanMv / 1000.0f) * kDividerRatio * gain_ + offset_v_;
  if (!isfinite(voltage) || voltage < 0.0f) return VoltageError::OutOfRange;
  reading.adc_mv = meanMv;
  reading.voltage_v = voltage;
  reading.low_voltage = voltage < kLowVoltageV;
  reading.valid = true;
  return VoltageError::None;
}

const char* VoltageSensor::errorName(VoltageError error) {
  switch (error) {
    case VoltageError::None: return "OK";
    case VoltageError::NotStarted: return "ADC_NOT_STARTED";
    case VoltageError::InvalidCalibration: return "INVALID_CALIBRATION";
    case VoltageError::AdcSaturated: return "ADC_SATURATED";
    case VoltageError::OutOfRange: return "ADC_OUT_OF_RANGE";
  }
  return "UNKNOWN";
}
}  // namespace aether

