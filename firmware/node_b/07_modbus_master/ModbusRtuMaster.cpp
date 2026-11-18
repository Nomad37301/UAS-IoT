#include "ModbusRtuMaster.h"

namespace aether {
namespace {
constexpr uint8_t kSlave = 1;
constexpr uint32_t kQuietUs = 5000;
constexpr uint32_t kTimeoutUs = 500000;
constexpr uint8_t kMaxRxPerPoll = 64;
// 8 byte x 10 bit / 9600 bps < 9 ms, ditambah 5 ms quiet sebelum request baru.
constexpr uint32_t kTxGuardUs = 14000;
}

uint16_t ModbusRtuMaster::crc16(const uint8_t* bytes, size_t count) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < count; ++i) {
    crc ^= bytes[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = static_cast<uint16_t>((crc & 1U) ? (crc >> 1U) ^ 0xA001U : crc >> 1U);
    }
  }
  return crc;
}

void ModbusRtuMaster::begin(uint32_t nowUs) {
  phase_ = Phase::Idle;
  result_ = ModbusResult{};
  lastActivityUs_ = nowUs;
  rxSize_ = 0;
  overflow_ = false;
  txGuard_ = false;
  started_ = true;
}

void ModbusRtuMaster::buildRequest(ModbusOperation operation) {
  tx_[0] = kSlave;
  tx_[1] = operation == ModbusOperation::ReadMetrics ? 0x03 : 0x06;
  tx_[2] = 0;
  tx_[3] = operation == ModbusOperation::ReadMetrics ? 0 : 5;
  tx_[4] = 0;
  tx_[5] = operation == ModbusOperation::ReadMetrics ? 5 : 1;
  const uint16_t crc = crc16(tx_, 6);
  tx_[6] = static_cast<uint8_t>(crc);  // CRC low byte lebih dahulu.
  tx_[7] = static_cast<uint8_t>(crc >> 8U);
}

ModbusStart ModbusRtuMaster::request(ModbusOperation operation, uint32_t nowUs) {
  if (!started_) return ModbusStart::NotStarted;
  if (operation != ModbusOperation::ReadMetrics && operation != ModbusOperation::RestartService) {
    return ModbusStart::InvalidOperation;
  }
  if (phase_ != Phase::Idle) return ModbusStart::Busy;
  result_ = ModbusResult{};
  result_.operation = operation;
  buildRequest(operation);
  queuedUs_ = nowUs;
  rxSize_ = 0;
  overflow_ = false;
  phase_ = Phase::WaitQuiet;
  return ModbusStart::Accepted;
}

void ModbusRtuMaster::finish(ModbusError error) {
  result_.error = error;
  if (error != ModbusError::None) result_.metrics = HostMetrics{};
  phase_ = Phase::Complete;
}

uint16_t ModbusRtuMaster::wordAt(size_t offset) const {
  return (static_cast<uint16_t>(rx_[offset]) << 8U) | rx_[offset + 1];
}

void ModbusRtuMaster::parse() {
  if (overflow_) { finish(ModbusError::Overflow); return; }
  if (rxSize_ < 5) { finish(ModbusError::FrameLength); return; }
  const uint16_t receivedCrc = static_cast<uint16_t>(rx_[rxSize_ - 2]) |
      (static_cast<uint16_t>(rx_[rxSize_ - 1]) << 8U);
  if (crc16(rx_, rxSize_ - 2) != receivedCrc) { finish(ModbusError::CrcMismatch); return; }
  if (rx_[0] != kSlave) { finish(ModbusError::WrongSlave); return; }
  if (rx_[1] == static_cast<uint8_t>(tx_[1] | 0x80U)) {
    if (rxSize_ != 5 || rx_[2] == 0) { finish(ModbusError::InvalidData); return; }
    result_.exception_code = rx_[2];
    finish(ModbusError::Exception);
    return;
  }
  if (rx_[1] != tx_[1]) { finish(ModbusError::WrongFunction); return; }
  if (result_.operation == ModbusOperation::RestartService) {
    if (rxSize_ != 8) { finish(ModbusError::FrameLength); return; }
    for (size_t i = 0; i < 6; ++i) {
      if (rx_[i] != tx_[i]) { finish(ModbusError::WriteEchoMismatch); return; }
    }
  } else {
    if (rxSize_ != 15) { finish(ModbusError::FrameLength); return; }
    if (rx_[2] != 10) { finish(ModbusError::InvalidData); return; }
    if (wordAt(5) > 100 || wordAt(7) > 100 || wordAt(9) > 1) {
      finish(ModbusError::InvalidData);
      return;
    }
    result_.metrics.cpu_temp_c = static_cast<float>(wordAt(3)) / 10.0f;
    result_.metrics.cpu_ram_pct = wordAt(5);
    result_.metrics.cpu_load_pct = wordAt(7);
    result_.metrics.service_running = wordAt(9) == 1;
    result_.metrics.uptime_s_low16 = wordAt(11);
    result_.metrics.valid = true;
  }
  finish(ModbusError::None);
}

void ModbusRtuMaster::poll(uint32_t nowUs) {
  if (!started_) return;
  if (txGuard_ && static_cast<uint32_t>(nowUs - lastTxQueuedUs_) >= kTxGuardUs) txGuard_ = false;
  // Deadline konservatif: bila polling terlambat, respons tidak dianggap tepat waktu.
  if (phase_ == Phase::Receive && static_cast<uint32_t>(nowUs - sentUs_) >= kTimeoutUs) {
    finish(ModbusError::Timeout);
  }
  for (uint8_t i = 0; i < kMaxRxPerPoll && serial_.available() > 0; ++i) {
    const int byte = serial_.read();
    if (byte < 0) break;
    lastActivityUs_ = nowUs;
    if (phase_ == Phase::Receive) {
      if (rxSize_ < sizeof(rx_)) rx_[rxSize_++] = static_cast<uint8_t>(byte);
      else overflow_ = true;
    }
    // Idle/WaitQuiet/Complete: buang byte lama, jangan jadikan jawaban request baru.
  }
  if (phase_ == Phase::WaitQuiet) {
    if (static_cast<uint32_t>(nowUs - queuedUs_) >= kTimeoutUs) {
      finish(ModbusError::BusUnavailable);
    } else if (!txGuard_ && serial_.available() == 0 &&
               static_cast<uint32_t>(nowUs - lastActivityUs_) >= kQuietUs &&
               serial_.availableForWrite() >= static_cast<int>(sizeof(tx_))) {
      sentUs_ = nowUs;
      lastTxQueuedUs_ = nowUs;
      txGuard_ = true;
      if (serial_.write(tx_, sizeof(tx_)) != sizeof(tx_)) finish(ModbusError::TxFailure);
      else phase_ = Phase::Receive;
    }
  } else if (phase_ == Phase::Receive && rxSize_ > 0 && serial_.available() == 0 &&
             static_cast<uint32_t>(nowUs - lastActivityUs_) >= kQuietUs) {
    parse();
  }
}

bool ModbusRtuMaster::takeResult(ModbusResult& result) {
  if (phase_ != Phase::Complete) return false;
  result = result_;
  phase_ = Phase::Idle;
  return true;
}

const char* ModbusRtuMaster::errorName(ModbusError error) {
  switch (error) {
    case ModbusError::None: return "OK";
    case ModbusError::BusUnavailable: return "BUS_OR_TX_UNAVAILABLE";
    case ModbusError::Timeout: return "TIMEOUT";
    case ModbusError::TxFailure: return "TX_FAILURE";
    case ModbusError::Overflow: return "RX_OVERFLOW";
    case ModbusError::FrameLength: return "FRAME_LENGTH";
    case ModbusError::CrcMismatch: return "CRC_MISMATCH";
    case ModbusError::WrongSlave: return "WRONG_SLAVE";
    case ModbusError::WrongFunction: return "WRONG_FUNCTION";
    case ModbusError::Exception: return "MODBUS_EXCEPTION";
    case ModbusError::InvalidData: return "INVALID_DATA";
    case ModbusError::WriteEchoMismatch: return "WRITE_ECHO_MISMATCH";
  }
  return "UNKNOWN";
}

const char* ModbusRtuMaster::startName(ModbusStart result) {
  switch (result) {
    case ModbusStart::Accepted: return "ACCEPTED";
    case ModbusStart::Busy: return "BUSY";
    case ModbusStart::NotStarted: return "NOT_STARTED";
    case ModbusStart::InvalidOperation: return "INVALID_OPERATION";
  }
  return "UNKNOWN";
}

}  // namespace aether

