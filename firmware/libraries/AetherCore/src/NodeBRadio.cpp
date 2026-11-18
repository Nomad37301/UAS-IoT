#include "NodeBRadio.h"

namespace aether {
namespace {
const uint8_t kToNodeC[6] = "AETH1";
const uint8_t kFromNodeC[6] = "AETH2";
constexpr uint32_t kTxTimeoutMs = 50;
}

bool NodeBRadio::begin(SPIClass& spi, uint8_t payloadSize, bool nodeB) {
  ready_ = pending_ = resultReady_ = false;
  if (payloadSize == 0 || payloadSize > 32) return false;
  if (!radio_.begin(&spi) || !radio_.isChipConnected()) return false;
  radio_.stopListening();
  if (!radio_.setDataRate(RF24_250KBPS)) return false;
  radio_.setChannel(76);
  radio_.setPALevel(RF24_PA_LOW);
  radio_.setAddressWidth(5);
  radio_.setCRCLength(RF24_CRC_16);
  radio_.setAutoAck(true);
  radio_.setRetries(3, 3);  // 1000 us antar retry, maksimal tiga retry hardware.
  radio_.disableAckPayload();
  radio_.disableDynamicPayloads();
  radio_.setPayloadSize(payloadSize);
  for (uint8_t pipe = 0; pipe < 6; ++pipe) radio_.closeReadingPipe(pipe);
  radio_.openWritingPipe(nodeB ? kToNodeC : kFromNodeC);
  radio_.openReadingPipe(1, nodeB ? kFromNodeC : kToNodeC);
  radio_.flush_rx();
  radio_.flush_tx();
  radio_.clearStatusFlags();
  payloadSize_ = payloadSize;
  radio_.startListening();
  ready_ = true;
  return true;
}

RadioSend NodeBRadio::send(const void* payload, uint8_t size, uint32_t nowMs) {
  if (!ready_) return RadioSend::NotReady;
  if (busy()) return RadioSend::Busy;
  if (!payload || size != payloadSize_) return RadioSend::SizeMismatch;
  radio_.stopListening();
  radio_.clearStatusFlags(RF24_TX_DS | RF24_TX_DF);
  radio_.flush_tx();
  if (!radio_.startWrite(payload, size, false)) {
    radio_.flush_tx();
    radio_.startListening();
    return RadioSend::FifoFailure;
  }
  startedMs_ = nowMs;
  pending_ = true;
  return RadioSend::Accepted;
}

void NodeBRadio::finish(RadioTx result) {
  // startWrite() hanya mem-pulse CE, tidak menahan CE selama menunggu hasil.
  radio_.flush_tx();
  radio_.startListening();
  pending_ = false;
  resultReady_ = true;
  result_ = result;
}

void NodeBRadio::poll(uint32_t nowMs) {
  if (!ready_ || !pending_) return;
  const uint8_t status = radio_.clearStatusFlags(RF24_TX_DS | RF24_TX_DF);
  if (status == 0xFF) {
    finish(RadioTx::SpiFailure);
    ready_ = false;
  } else if (status & RF24_TX_DF) {
    finish(RadioTx::MaxRetries);
  } else if (status & RF24_TX_DS) {
    finish(RadioTx::Acked);
  } else if (static_cast<uint32_t>(nowMs - startedMs_) >= kTxTimeoutMs) {
    finish(RadioTx::Timeout);
  }
}

bool NodeBRadio::receive(void* payload, uint8_t size) {
  if (!ready_ || pending_ || !payload || size != payloadSize_) return false;
  uint8_t pipe = 0;
  if (!radio_.available(&pipe)) return false;
  radio_.read(payload, size);
  return pipe == 1;
}

bool NodeBRadio::takeTxResult(RadioTx& result) {
  if (!resultReady_) return false;
  result = result_;
  resultReady_ = false;
  return true;
}

const char* NodeBRadio::sendName(RadioSend result) {
  switch (result) {
    case RadioSend::Accepted: return "ACCEPTED";
    case RadioSend::NotReady: return "NOT_READY";
    case RadioSend::Busy: return "BUSY";
    case RadioSend::SizeMismatch: return "SIZE_MISMATCH";
    case RadioSend::FifoFailure: return "FIFO_FAILURE";
  }
  return "UNKNOWN";
}

const char* NodeBRadio::txName(RadioTx result) {
  switch (result) {
    case RadioTx::Acked: return "ACKED";
    case RadioTx::MaxRetries: return "MAX_RETRIES";
    case RadioTx::Timeout: return "TIMEOUT";
    case RadioTx::SpiFailure: return "SPI_FAILURE";
  }
  return "UNKNOWN";
}

}  // namespace aether
