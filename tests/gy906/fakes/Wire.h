#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

// Bus simulasi untuk memeriksa urutan transaksi driver, bukan akses perangkat.
class TwoWire {
 public:
  struct Reply {
    uint8_t reg;
    std::vector<uint8_t> bytes;
    uint8_t txStatus;
  };
  std::deque<Reply> replies;
  bool acceptWrite = true;
  unsigned starts = 0;

  void beginTransmission(uint8_t address) {
    assert(address == 0x5A);
    assert(!replies.empty());
    ++starts;
  }
  size_t write(uint8_t value) {
    assert(value == replies.front().reg);
    return acceptWrite ? 1 : 0;
  }
  uint8_t endTransmission(bool stop) {
    if (!acceptWrite) { assert(stop); return 0; }
    assert(!stop);  // Perintah harus diikuti repeated START.
    const uint8_t status = replies.front().txStatus;
    if (status != 0) replies.pop_front();
    return status;
  }
  size_t requestFrom(uint8_t address, size_t count, bool stop) {
    assert(address == 0x5A && count == 3 && stop);
    rx_ = replies.front().bytes;
    replies.pop_front();
    position_ = 0;
    return rx_.size();
  }
  int available() { return static_cast<int>(rx_.size() - position_); }
  int read() { return available() ? rx_[position_++] : -1; }

 private:
  std::vector<uint8_t> rx_;
  size_t position_ = 0;
};

