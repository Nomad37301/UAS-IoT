#pragma once

#include "SPI.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <vector>

constexpr uint8_t RF24_250KBPS = 2;
constexpr uint8_t RF24_PA_LOW = 1;
constexpr uint8_t RF24_CRC_16 = 2;
constexpr uint8_t RF24_TX_DS = 0x20;
constexpr uint8_t RF24_TX_DF = 0x10;
constexpr uint8_t RF24_IRQ_ALL = 0x70;

class RF24 {
 public:
  bool beginOk = true, present = true, rateOk = true, writeOk = true;
  bool listening = false;
  uint8_t status = 0x0E;
  uint8_t payloadSize = 0;
  unsigned sentCount = 0;
  unsigned flushTxCount = 0;
  uint8_t closedPipes = 0;
  std::vector<uint8_t> lastSent;
  struct Received { uint8_t pipe; std::vector<uint8_t> bytes; };
  std::deque<Received> incoming;

  bool begin(SPIClass* spi) { assert(spi); return beginOk; }
  bool isChipConnected() { return present; }
  bool setDataRate(uint8_t rate) { assert(rate == RF24_250KBPS); return rateOk; }
  void setChannel(uint8_t channel) { assert(channel == 76); }
  void setPALevel(uint8_t level) { assert(level == RF24_PA_LOW); }
  void setAddressWidth(uint8_t width) { assert(width == 5); }
  void setCRCLength(uint8_t crc) { assert(crc == RF24_CRC_16); }
  void setAutoAck(bool enabled) { assert(enabled); }
  void setRetries(uint8_t delay, uint8_t count) { assert(delay == 3 && count == 3); }
  void disableAckPayload() {}
  void disableDynamicPayloads() {}
  void setPayloadSize(uint8_t size) { payloadSize = size; }
  void closeReadingPipe(uint8_t pipe) { closedPipes |= 1U << pipe; }
  void openWritingPipe(const uint8_t* pipe) { assert(std::memcmp(pipe, "AETH1", 5) == 0); }
  void openReadingPipe(uint8_t number, const uint8_t* pipe) {
    assert(number == 1 && std::memcmp(pipe, "AETH2", 5) == 0);
  }
  void stopListening() { listening = false; }
  void startListening() { listening = true; }
  void flush_rx() { incoming.clear(); }
  void flush_tx() { ++flushTxCount; }
  uint8_t clearStatusFlags(uint8_t mask = RF24_IRQ_ALL) {
    const uint8_t result = status;
    status &= static_cast<uint8_t>(~mask);
    return result;
  }
  bool startWrite(const void* bytes, uint8_t length, bool multicast) {
    assert(!listening && !multicast && length == payloadSize);
    ++sentCount;
    const auto* data = static_cast<const uint8_t*>(bytes);
    lastSent.assign(data, data + length);
    return writeOk;
  }
  bool available(uint8_t* pipe) {
    if (incoming.empty()) return false;
    *pipe = incoming.front().pipe;
    return true;
  }
  void read(void* bytes, uint8_t length) {
    assert(listening && !incoming.empty() && incoming.front().bytes.size() == length);
    std::memcpy(bytes, incoming.front().bytes.data(), length);
    incoming.pop_front();
  }
};

