#include "VoltageSensor.h"

#include <cassert>
#include <cmath>
#include <deque>
#include <iostream>
#include <limits>

namespace {
std::deque<uint16_t> rawValues;
std::deque<uint32_t> mvValues;
unsigned configurationSteps = 0;

void setSamples(uint32_t mv, uint16_t raw = 1300) {
  rawValues.assign(16, raw);
  mvValues.assign(16, mv);
}

void expectInvalid(const aether::VoltageReading& reading) {
  assert(!reading.valid);
  assert(!reading.low_voltage);
  assert(std::isnan(reading.voltage_v));
  assert(std::isnan(reading.adc_mv));
}

void expectNear(float actual, float expected) {
  assert(std::fabs(actual - expected) < 0.0001f);
}
}  // namespace

void pinMode(uint8_t pin, uint8_t mode) {
  assert(pin == 4 && mode == INPUT);
  ++configurationSteps;
}
void analogReadResolution(uint8_t bits) {
  assert(bits == 12);
  ++configurationSteps;
}
void analogSetPinAttenuation(uint8_t pin, adc_attenuation_t attenuation) {
  assert(pin == 4 && attenuation == ADC_11db);
  ++configurationSteps;
}
uint16_t analogRead(uint8_t pin) {
  assert(pin == 4 && !rawValues.empty());
  const uint16_t value = rawValues.front();
  rawValues.pop_front();
  return value;
}
uint32_t analogReadMilliVolts(uint8_t pin) {
  assert(pin == 4 && !mvValues.empty());
  const uint32_t value = mvValues.front();
  mvValues.pop_front();
  return value;
}

int main() {
  using aether::VoltageError;
  using aether::VoltageReading;
  using aether::VoltageSensor;
  VoltageSensor sensor(4);
  VoltageReading reading;
  assert(sensor.read(reading) == VoltageError::NotStarted);
  expectInvalid(reading);
  sensor.begin();
  assert(configurationSteps == 3);

  setSamples(1008);
  assert(sensor.read(reading) == VoltageError::None);
  assert(reading.valid && !reading.low_voltage);
  expectNear(reading.voltage_v, 5.04f);
  assert(rawValues.empty() && mvValues.empty());

  // Rata-rata 16 konversi harus tetap menyimpan pecahan mV.
  setSamples(1000);
  mvValues.front() = 1001;
  assert(sensor.read(reading) == VoltageError::None);
  expectNear(reading.adc_mv, 1000.0625f);
  expectNear(reading.voltage_v, 5.0003125f);

  setSamples(900);
  assert(sensor.read(reading) == VoltageError::None);
  expectNear(reading.voltage_v, 4.5f);
  assert(!reading.low_voltage);
  setSamples(899);
  assert(sensor.read(reading) == VoltageError::None);
  assert(reading.low_voltage);

  // Nol tetap dilaporkan rendah; tidak mengklaim bisa mendeteksi kabel putus.
  setSamples(0, 0);
  assert(sensor.read(reading) == VoltageError::None);
  expectNear(reading.voltage_v, 0.0f);
  assert(reading.valid && reading.low_voltage);

  // Clipping di tengah window membatalkan seluruh sampel dan data lama.
  setSamples(1000);
  rawValues[8] = 4095;
  assert(sensor.read(reading) == VoltageError::AdcSaturated);
  expectInvalid(reading);
  setSamples(3100);
  assert(sensor.read(reading) == VoltageError::OutOfRange);
  expectInvalid(reading);

  setSamples(1000);
  assert(sensor.read(reading) == VoltageError::None);
  assert(reading.valid && !reading.low_voltage);

  VoltageSensor calibrated(4, 1.02f, 0.01f);
  calibrated.begin();
  setSamples(1000);
  assert(calibrated.read(reading) == VoltageError::None);
  expectNear(reading.voltage_v, 5.11f);

  for (float gain : {0.0f, -1.0f, std::numeric_limits<float>::infinity(),
                     std::numeric_limits<float>::quiet_NaN()}) {
    VoltageSensor bad(4, gain);
    bad.begin();
    assert(bad.read(reading) == VoltageError::InvalidCalibration);
    expectInvalid(reading);
  }
  VoltageSensor badOffset(4, 1.0f, NAN);
  badOffset.begin();
  assert(badOffset.read(reading) == VoltageError::InvalidCalibration);
  expectInvalid(reading);

  VoltageSensor negative(4, 1.0f, -6.0f);
  negative.begin();
  setSamples(1000);
  assert(negative.read(reading) == VoltageError::OutOfRange);
  expectInvalid(reading);

  std::cout << "PASS: ADC configuration, averaging, 1:5 conversion, 4.5V boundary, "
               "calibration, clipping, invalidation, recovery\n";
}

