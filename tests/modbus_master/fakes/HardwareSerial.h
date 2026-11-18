#pragma once

#include <stdint.h>
#include <stddef.h>
#include <deque>
#include <vector>

class HardwareSerial {
 public:
  std::deque<uint8_t> rx;
  std::vector<std::vector<uint8_t>> sent;
  int capacity = 64;
  size_t writeLimit = 8;
  int available() { return static_cast<int>(rx.size()); }
  int availableForWrite() { return capacity; }
  int read() {
    if (rx.empty()) return -1;
    const uint8_t value = rx.front();
    rx.pop_front();
    return value;
  }
  size_t write(const uint8_t* data, size_t count) {
    const size_t actual = count < writeLimit ? count : writeLimit;
    sent.emplace_back(data, data + actual);
    return actual;
  }
  void inject(const std::vector<uint8_t>& data) { rx.insert(rx.end(), data.begin(), data.end()); }
};

