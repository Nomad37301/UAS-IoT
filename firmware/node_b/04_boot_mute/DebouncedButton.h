#pragma once

#include <Arduino.h>

namespace aether {

// Tombol aktif LOW. Panggil begin/update dari satu loop pemilik.
class DebouncedButton {
 public:
  explicit DebouncedButton(uint8_t pin) : pin_(pin) {}

  void begin(uint32_t nowMs);
  // true tepat sekali per penekanan stabil, setelah pelepasan stabil.
  bool update(uint32_t nowMs);
  bool pressed() const { return settled_ && stablePressed_; }
  bool armed() const { return armed_; }

 private:
  uint8_t pin_;
  bool started_ = false;
  bool candidatePressed_ = false;
  bool stablePressed_ = false;
  bool settled_ = false;
  bool armed_ = false;
  uint32_t candidateSinceMs_ = 0;
};

}  // namespace aether

