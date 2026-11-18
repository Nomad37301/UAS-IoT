#include "Mlx90614.h"

namespace aether {

uint8_t Mlx90614::crc8(const uint8_t* bytes, size_t count) {
  uint8_t crc = 0;
  for (size_t i = 0; i < count; ++i) {
    crc ^= bytes[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = static_cast<uint8_t>((crc & 0x80U)
          ? (crc << 1U) ^ 0x07U : crc << 1U);
    }
  }
  return crc;
}

TemperatureError Mlx90614::readTemperature(uint8_t reg, float& temperature) {
  temperature = NAN;
  wire_.beginTransmission(address_);
  if (wire_.write(reg) != 1) {
    wire_.endTransmission(true);
    return TemperatureError::TxBuffer;
  }

  // Pertahankan transaksi agar requestFrom() membuat repeated START SMBus.
  const uint8_t status = wire_.endTransmission(false);
  if (status != 0) {
    if (status == 2 || status == 3) return TemperatureError::Nack;
    if (status == 5) return TemperatureError::Timeout;
    return TemperatureError::Bus;
  }

  const size_t received = wire_.requestFrom(address_, static_cast<size_t>(3), true);
  if (received != 3 || wire_.available() != 3) {
    while (wire_.available() > 0) wire_.read();
    // ESP32 dapat melaporkan NACK/timeout di requestFrom() sebagai nol byte.
    // Wire tidak mengekspos penyebab rinci kegagalan pada tahap baca ini.
    return TemperatureError::ShortRead;
  }

  const uint8_t low = static_cast<uint8_t>(wire_.read());
  const uint8_t high = static_cast<uint8_t>(wire_.read());
  const uint8_t pec = static_cast<uint8_t>(wire_.read());
  const uint8_t transaction[] = {
      static_cast<uint8_t>(address_ << 1U), reg,
      static_cast<uint8_t>((address_ << 1U) | 1U), low, high};
  if (crc8(transaction, sizeof(transaction)) != pec) {
    return TemperatureError::PecMismatch;
  }

  const uint16_t raw = static_cast<uint16_t>(low) |
                       (static_cast<uint16_t>(high) << 8U);
  if ((raw & 0x8000U) != 0) return TemperatureError::SensorFault;
  temperature = static_cast<float>(raw) * 0.02f - 273.15f;
  return TemperatureError::None;
}

TemperatureError Mlx90614::read(TemperatureReading& reading) {
  // Selalu batalkan data lama. Pasangan hanya valid bila kedua register lolos.
  reading = TemperatureReading{};
  float ambient = NAN;
  float object = NAN;
  TemperatureError error = readTemperature(0x06, ambient);
  if (error != TemperatureError::None) return error;
  error = readTemperature(0x07, object);
  if (error != TemperatureError::None) return error;
  reading.ir_amb_c = ambient;
  reading.ir_obj_c = object;
  reading.valid = true;
  return TemperatureError::None;
}

const char* Mlx90614::errorName(TemperatureError error) {
  switch (error) {
    case TemperatureError::None: return "OK";
    case TemperatureError::BusNotReady: return "I2C_INIT_FAILED";
    case TemperatureError::TxBuffer: return "I2C_TX_BUFFER_ERROR";
    case TemperatureError::Nack: return "I2C_NACK";
    case TemperatureError::Bus: return "I2C_BUS_ERROR";
    case TemperatureError::Timeout: return "I2C_TIMEOUT";
    case TemperatureError::ShortRead: return "I2C_SHORT_READ_OR_NO_RESPONSE";
    case TemperatureError::PecMismatch: return "PEC_MISMATCH";
    case TemperatureError::SensorFault: return "SENSOR_ERROR_FLAG";
  }
  return "UNKNOWN";
}

}  // namespace aether

