#include "ModbusRtuMaster.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

using namespace aether;

static std::vector<uint8_t> frame(std::vector<uint8_t> bytes) {
  const uint16_t crc = ModbusRtuMaster::crc16(bytes.data(), bytes.size());
  bytes.push_back(static_cast<uint8_t>(crc));
  bytes.push_back(static_cast<uint8_t>(crc >> 8));
  return bytes;
}

static std::vector<uint8_t> metricsFrame() {
  // CPU 49.2 C, RAM 34%, load 20%, service running, uptime low16 0xFFFF.
  return frame({1, 3, 10, 0x01, 0xEC, 0, 34, 0, 20, 0, 1, 0xFF, 0xFF});
}

static ModbusResult exchange(const std::vector<uint8_t>& response,
                             ModbusOperation op = ModbusOperation::ReadMetrics) {
  HardwareSerial port;
  ModbusRtuMaster master(port);
  master.begin(0);
  assert(master.request(op, 0) == ModbusStart::Accepted);
  master.poll(4999);
  assert(port.sent.empty());
  master.poll(5000);
  assert(port.sent.size() == 1);
  const std::vector<uint8_t> expectedRead = {1, 3, 0, 0, 0, 5, 0x85, 0xC9};
  const std::vector<uint8_t> expectedWrite = {1, 6, 0, 5, 0, 1, 0x58, 0x0B};
  assert(port.sent[0] == (op == ModbusOperation::ReadMetrics ? expectedRead : expectedWrite));
  assert(master.request(op, 6000) == ModbusStart::Busy);
  port.inject(response);
  master.poll(25000);
  // Masing-masing poll dibatasi 64 byte; habiskan fixture oversize pada waktu ini.
  while (port.available() > 0) master.poll(25000);
  ModbusResult result;
  assert(!master.takeResult(result));
  master.poll(29999);
  assert(!master.takeResult(result));
  master.poll(30000);
  assert(master.busy());  // Hasil tidak boleh tertimpa sebelum diambil.
  assert(master.takeResult(result));
  assert(!master.busy() && !master.takeResult(result));
  return result;
}

static void expectError(const std::vector<uint8_t>& bytes, ModbusError expected,
                         ModbusOperation op = ModbusOperation::ReadMetrics) {
  const auto result = exchange(bytes, op);
  assert(result.error == expected);
  assert(!result.metrics.valid && std::isnan(result.metrics.cpu_temp_c));
}

int main() {
  // Vektor CRC standar yang terpisah dari pembentuk fixture respons.
  const uint8_t golden[] = {1, 3, 0, 0, 0, 10};
  assert(ModbusRtuMaster::crc16(golden, sizeof(golden)) == 0xCDC5);
  auto good = exchange(metricsFrame());
  assert(good.error == ModbusError::None && good.metrics.valid);
  assert(std::fabs(good.metrics.cpu_temp_c - 49.2f) < 0.001f);
  assert(good.metrics.cpu_ram_pct == 34 && good.metrics.cpu_load_pct == 20);
  assert(good.metrics.service_running && good.metrics.uptime_s_low16 == 65535);

  auto broken = metricsFrame();
  broken[4] ^= 1;
  expectError(broken, ModbusError::CrcMismatch);
  auto body = std::vector<uint8_t>{2, 3, 10, 1, 0xEC, 0, 34, 0, 20, 0, 1, 0, 10};
  expectError(frame(body), ModbusError::WrongSlave);
  body[0] = 1; body[1] = 4;
  expectError(frame(body), ModbusError::WrongFunction);
  body[1] = 3; body[2] = 8;
  expectError(frame(body), ModbusError::InvalidData);
  body[2] = 10; body[6] = 101;
  expectError(frame(body), ModbusError::InvalidData);
  body[6] = 34; body[10] = 2;
  expectError(frame(body), ModbusError::InvalidData);
  expectError({1, 3, 10}, ModbusError::FrameLength);
  expectError(frame({1, 3, 2, 0, 1}), ModbusError::FrameLength);
  const auto exception = exchange(frame({1, 0x83, 2}));
  assert(exception.error == ModbusError::Exception && exception.exception_code == 2);
  expectError(frame({1, 0x83, 0}), ModbusError::InvalidData);
  expectError(std::vector<uint8_t>(80, 0xAA), ModbusError::Overflow);

  const auto ack = exchange({1, 6, 0, 5, 0, 1, 0x58, 0x0B}, ModbusOperation::RestartService);
  assert(ack.error == ModbusError::None && !ack.metrics.valid);
  expectError(frame({1, 6, 0, 6, 0, 1}), ModbusError::WriteEchoMismatch,
              ModbusOperation::RestartService);
  expectError(frame({1, 0x86, 4}), ModbusError::Exception, ModbusOperation::RestartService);

  HardwareSerial port;
  ModbusRtuMaster master(port);
  assert(master.request(ModbusOperation::ReadMetrics, 0) == ModbusStart::NotStarted);
  master.begin(0);
  assert(master.request(static_cast<ModbusOperation>(255), 0) == ModbusStart::InvalidOperation);
  master.poll(5000);
  assert(port.sent.empty());  // Operasi tidak dikenal tidak boleh menjadi write.
  assert(master.request(ModbusOperation::ReadMetrics, 5000) == ModbusStart::Accepted);
  master.poll(5000);
  master.poll(504999);
  ModbusResult result;
  assert(!master.takeResult(result));
  master.poll(505000);
  assert(master.takeResult(result) && result.error == ModbusError::Timeout);
  assert(!result.metrics.valid);
  assert(port.sent.size() == 1);  // Tidak ada retry otomatis.

  // Frame terlambat dibuang sebelum request berikutnya dikirim.
  port.inject(metricsFrame());
  assert(master.request(ModbusOperation::ReadMetrics, 506000) == ModbusStart::Accepted);
  master.poll(506000);
  assert(port.sent.size() == 1);
  master.poll(511000);
  assert(port.sent.size() == 2);
  auto validFrame = metricsFrame();
  port.inject(std::vector<uint8_t>(validFrame.begin(), validFrame.begin() + 5));
  master.poll(531000);
  port.inject(std::vector<uint8_t>(validFrame.begin() + 5, validFrame.end()));
  master.poll(532000);
  master.poll(537000);
  assert(master.takeResult(result) && result.metrics.valid);
  // Sampel sesudahnya gagal: jangan mengambil metrik lama.
  assert(master.request(ModbusOperation::ReadMetrics, 540000) == ModbusStart::Accepted);
  master.poll(540000);
  master.poll(1040000);
  assert(master.takeResult(result) && !result.metrics.valid);

  // TX yang gagal sebagian tetap dibatasi guard agar frame baru tidak menimpa TX lama.
  master.begin(0);
  port.sent.clear(); port.writeLimit = 3;
  master.request(ModbusOperation::RestartService, 0);
  master.poll(5000);
  assert(master.takeResult(result) && result.error == ModbusError::TxFailure);
  port.writeLimit = 8;
  master.request(ModbusOperation::ReadMetrics, 6000);
  master.poll(10000);
  assert(port.sent.size() == 1);
  master.poll(19000);
  assert(port.sent.size() == 2);

  // Tidak ada kapasitas TX: timeout tanpa mengirim apa pun.
  master.begin(0); port.sent.clear(); port.capacity = 0;
  master.request(ModbusOperation::ReadMetrics, 0);
  master.poll(500000);
  assert(master.takeResult(result) && result.error == ModbusError::BusUnavailable);
  assert(port.sent.empty());

  // Deadline dan quiet gap tetap benar melintasi overflow micros().
  port.capacity = 64;
  const uint32_t start = UINT32_MAX - 1000U;
  master.begin(start);
  master.request(ModbusOperation::ReadMetrics, start);
  master.poll(static_cast<uint32_t>(start + 5000U));
  port.inject(metricsFrame());
  master.poll(static_cast<uint32_t>(start + 25000U));
  master.poll(static_cast<uint32_t>(start + 30000U));
  assert(master.takeResult(result) && result.metrics.valid);
  std::cout << "PASS: golden CRC/requests, register decoding, validation, exceptions, "
               "FC06 echo, timeout, stale invalidation, TX guard, rollover\n";
}

