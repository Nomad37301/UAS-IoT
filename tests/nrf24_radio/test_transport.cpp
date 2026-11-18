#include "NodeBRadio.h"

#include <cassert>
#include <iostream>

int main() {
  using namespace aether;
  SPIClass spi;
  RF24 hardware;
  NodeBRadio link(hardware);
  // 16 menguji API raw generik. Kontrak AETHER 32 byte diuji di test_packet.cpp.
  uint8_t payload[16] = {0xAE, 1, 2, 3};
  uint8_t received[16] = {};
  RadioTx result;
  assert(link.send(payload, 16, 0) == RadioSend::NotReady);
  link.poll(100);
  assert(!link.receive(received, 16));
  assert(!link.begin(spi, 0) && !link.begin(spi, 33));
  hardware.present = false;
  assert(!link.begin(spi, 16));
  hardware.present = true; hardware.rateOk = false;
  assert(!link.begin(spi, 16));
  hardware.rateOk = true;
  assert(link.begin(spi, 16));
  assert(link.ready() && hardware.listening && hardware.closedPipes == 0x3F);
  assert(link.send(nullptr, 16, 0) == RadioSend::SizeMismatch);
  assert(link.send(payload, 15, 0) == RadioSend::SizeMismatch);

  hardware.incoming.push_back({1, std::vector<uint8_t>(payload, payload + 16)});
  hardware.status = RF24_TX_DS;  // Status lama harus dibersihkan sebelum TX.
  assert(link.send(payload, 16, 100) == RadioSend::Accepted);
  assert(link.busy() && !hardware.listening);
  assert(link.send(payload, 16, 101) == RadioSend::Busy);
  assert(!link.receive(received, 16));
  link.poll(149);
  assert(!link.takeTxResult(result));
  hardware.status = RF24_TX_DS;
  link.poll(149);
  assert(hardware.listening && link.busy());
  assert(link.send(payload, 16, 150) == RadioSend::Busy);
  assert(link.takeTxResult(result) && result == RadioTx::Acked);
  assert(!link.takeTxResult(result));
  assert(link.receive(received, 16));
  assert(std::memcmp(received, payload, 16) == 0);
  assert(!link.receive(received, 16));

  assert(link.send(payload, 16, 200) == RadioSend::Accepted);
  hardware.status = RF24_TX_DF;
  link.poll(201);
  assert(link.takeTxResult(result) && result == RadioTx::MaxRetries);
  assert(hardware.listening && hardware.sentCount == 2);
  link.poll(500);
  assert(hardware.sentCount == 2);  // Tidak retry software secara otomatis.

  const uint32_t start = UINT32_MAX - 20U;
  assert(link.send(payload, 16, start) == RadioSend::Accepted);
  link.poll(static_cast<uint32_t>(start + 49U));
  assert(!link.takeTxResult(result));
  link.poll(static_cast<uint32_t>(start + 50U));
  assert(link.takeTxResult(result) && result == RadioTx::Timeout);
  assert(hardware.listening);

  hardware.writeOk = false;
  assert(link.send(payload, 16, 1000) == RadioSend::FifoFailure);
  assert(!link.busy() && hardware.listening);
  hardware.writeOk = true;
  assert(link.send(payload, 16, 1100) == RadioSend::Accepted);
  hardware.status = 0xFF;
  link.poll(1101);
  assert(link.takeTxResult(result) && result == RadioTx::SpiFailure);
  assert(!link.ready() && link.send(payload, 16, 1102) == RadioSend::NotReady);
  hardware.status = 0x0E;
  assert(link.begin(spi, 16));
  assert(!link.busy() && link.ready());
  std::cout << "PASS: radio contract, init failures, exact size, async ACK/failure, "
               "RX retained, FIFO recovery, no software retry, timeout, rollover\n";
}
