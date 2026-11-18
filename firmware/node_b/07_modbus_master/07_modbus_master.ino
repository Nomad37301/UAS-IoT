#include <Arduino.h>

#include "ModbusRtuMaster.h"
#include "NodeBPins.h"

namespace {
constexpr uint32_t kPollPeriodMs = 2000;
aether::ModbusRtuMaster master(Serial1);
bool automaticPolling = true;
uint32_t lastPollMs = 0;

void printHelp() {
  Serial.println("Commands: p=read registers, r=restart service FC06, a=toggle auto read, ?=help; auto period=2s");
}

void startOperation(aether::ModbusOperation operation) {
  const auto status = master.request(operation, micros());
  Serial.printf("{\"event\":\"REQUEST\",\"operation\":\"%s\",\"result\":\"%s\"}\n",
                operation == aether::ModbusOperation::ReadMetrics ? "READ_METRICS" : "RESTART_SERVICE",
                aether::ModbusRtuMaster::startName(status));
}

void printResult(const aether::ModbusResult& result) {
  const bool ok = result.error == aether::ModbusError::None;
  Serial.printf("{\"system\":\"AETHER\",\"node\":\"nodeB\",\"component\":\"MODBUS_MASTER\","
                "\"operation\":\"%s\",\"modbus_ok\":%s,\"error\":\"%s\",\"exception_code\":%u,",
                result.operation == aether::ModbusOperation::ReadMetrics ? "READ_METRICS" : "RESTART_SERVICE",
                ok ? "true" : "false", aether::ModbusRtuMaster::errorName(result.error),
                static_cast<unsigned>(result.exception_code));
  if (result.metrics.valid) {
    Serial.printf("\"cpu_temp_c\":%.1f,\"cpu_ram_pct\":%u,\"cpu_load_pct\":%u,"
                  "\"service_running\":%s,\"uptime_s_low16\":%u,",
                  result.metrics.cpu_temp_c, static_cast<unsigned>(result.metrics.cpu_ram_pct),
                  static_cast<unsigned>(result.metrics.cpu_load_pct),
                  result.metrics.service_running ? "true" : "false",
                  static_cast<unsigned>(result.metrics.uptime_s_low16));
  } else {
    Serial.print("\"cpu_temp_c\":null,\"cpu_ram_pct\":null,\"cpu_load_pct\":null,"
                 "\"service_running\":null,\"uptime_s_low16\":null,");
  }
  // Echo FC06 hanya mengakui write; bukan bukti service kembali sehat.
  Serial.printf("\"restart_ack\":%s}\n",
                result.operation == aether::ModbusOperation::RestartService && ok ? "true" : "false");
}
}  // namespace

void setup() {
  Serial.begin(115200);
  Serial1.setRxBufferSize(256);
  Serial1.setTxBufferSize(64);
  Serial1.begin(MODBUS_BAUD, SERIAL_8N1, PIN_MODBUS_RX, PIN_MODBUS_TX);
  if (Serial1) {
    Serial1.setRxFIFOFull(1);
    Serial1.setRxTimeout(1);
    master.begin(micros());
  } else {
    automaticPolling = false;
    Serial.println("{\"error\":\"UART_INIT_FAILED\"}");
  }
  Serial.println("AETHER Node B | Bagian 07: Modbus RTU | UART1 TX=17 RX=18 9600 8N1 SLAVE=1");
  printHelp();
  lastPollMs = millis();
}

void loop() {
  master.poll(micros());
  aether::ModbusResult result;
  if (master.takeResult(result)) printResult(result);
  for (uint8_t i = 0; i < 16 && Serial.available() > 0; ++i) {
    master.poll(micros());
    const char command = static_cast<char>(Serial.read());
    switch (command) {
      case 'p': startOperation(aether::ModbusOperation::ReadMetrics); break;
      case 'r': startOperation(aether::ModbusOperation::RestartService); break;
      case 'a':
        automaticPolling = !automaticPolling;
        Serial.printf("{\"auto_poll\":%s}\n", automaticPolling ? "true" : "false");
        break;
      case '?': printHelp(); break;
      case '\r': case '\n': case ' ': case '\t': break;
      default: Serial.println("{\"error\":\"UNKNOWN_COMMAND\"}"); break;
    }
  }
  const uint32_t nowMs = millis();
  if (automaticPolling && !master.busy() && static_cast<uint32_t>(nowMs - lastPollMs) >= kPollPeriodMs) {
    lastPollMs = nowMs;
    startOperation(aether::ModbusOperation::ReadMetrics);
  }
  delay(1);
}
