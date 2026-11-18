#pragma once

#include <HardwareSerial.h>
#include <math.h>

namespace aether {

enum class ModbusOperation : uint8_t { ReadMetrics, RestartService };
enum class ModbusStart : uint8_t { Accepted, Busy, NotStarted, InvalidOperation };
enum class ModbusError : uint8_t {
  None, BusUnavailable, Timeout, TxFailure, Overflow, FrameLength,
  CrcMismatch, WrongSlave, WrongFunction, Exception, InvalidData, WriteEchoMismatch
};

struct HostMetrics {
  bool valid = false;
  float cpu_temp_c = NAN;
  uint16_t cpu_ram_pct = 0;
  uint16_t cpu_load_pct = 0;
  bool service_running = false;
  uint16_t uptime_s_low16 = 0;
};

struct ModbusResult {
  ModbusOperation operation = ModbusOperation::ReadMetrics;
  ModbusError error = ModbusError::None;
  uint8_t exception_code = 0;
  HostMetrics metrics;
};

// Subset AETHER: slave 1, FC03 reg 0..4, FC06 reg 5=1.
// UART dikonfigurasi pemiliknya; begin/poll/request/takeResult pada satu loop.
class ModbusRtuMaster {
 public:
  explicit ModbusRtuMaster(HardwareSerial& serial) : serial_(serial) {}
  void begin(uint32_t nowUs);
  ModbusStart request(ModbusOperation operation, uint32_t nowUs);
  void poll(uint32_t nowUs);
  bool busy() const { return phase_ != Phase::Idle; }
  bool takeResult(ModbusResult& result);

  static uint16_t crc16(const uint8_t* bytes, size_t count);
  static const char* errorName(ModbusError error);
  static const char* startName(ModbusStart result);

 private:
  enum class Phase : uint8_t { Idle, WaitQuiet, Receive, Complete };
  void finish(ModbusError error);
  void parse();
  void buildRequest(ModbusOperation operation);
  uint16_t wordAt(size_t offset) const;

  HardwareSerial& serial_;
  bool started_ = false;
  Phase phase_ = Phase::Idle;
  ModbusResult result_;
  uint8_t tx_[8] = {};
  uint8_t rx_[32] = {};
  size_t rxSize_ = 0;
  bool overflow_ = false;
  uint32_t queuedUs_ = 0;
  uint32_t sentUs_ = 0;
  uint32_t lastActivityUs_ = 0;
  uint32_t lastTxQueuedUs_ = 0;
  bool txGuard_ = false;
};

}  // namespace aether

