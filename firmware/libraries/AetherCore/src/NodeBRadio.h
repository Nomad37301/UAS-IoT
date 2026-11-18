#pragma once

#include <RF24.h>
#include <SPI.h>

namespace aether {

enum class RadioSend : uint8_t { Accepted, NotReady, Busy, SizeMismatch, FifoFailure };
enum class RadioTx : uint8_t { Acked, MaxRetries, Timeout, SpiFailure };

// Transport radio saja. Interpretasi payload/perintah dilakukan pemilik driver.
class NodeBRadio {
 public:
  explicit NodeBRadio(RF24& radio) : radio_(radio) {}
  bool begin(SPIClass& spi, uint8_t payloadSize, bool nodeB = true);
  RadioSend send(const void* payload, uint8_t size, uint32_t nowMs);
  void poll(uint32_t nowMs);
  bool receive(void* payload, uint8_t size);
  bool takeTxResult(RadioTx& result);
  bool ready() const { return ready_; }
  bool busy() const { return pending_ || resultReady_; }
  static const char* sendName(RadioSend result);
  static const char* txName(RadioTx result);

 private:
  void finish(RadioTx result);

  RF24& radio_;
  uint8_t payloadSize_ = 0;
  bool ready_ = false;
  bool pending_ = false;
  bool resultReady_ = false;
  uint32_t startedMs_ = 0;
  RadioTx result_ = RadioTx::Timeout;
};

}  // namespace aether

