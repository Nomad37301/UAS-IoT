#pragma once

#include <Arduino.h>

namespace aether {

enum class BuzzerPattern : uint8_t { Off, Test, Warning, Critical };

// Panggil update() sesering mungkin dari loop, bukan dari interrupt.
class ActiveBuzzer {
 public:
  explicit ActiveBuzzer(uint8_t pin) : pin_(pin) {}

  void begin();
  void setPattern(BuzzerPattern pattern, uint32_t nowMs);
  void setMuted(bool muted, uint32_t nowMs);
  void update(uint32_t nowMs);

  BuzzerPattern pattern() const { return pattern_; }
  bool muted() const { return muted_; }
  bool outputHigh() const { return outputHigh_; }
  static const char* patternName(BuzzerPattern pattern);

 private:
  void writeOutput(bool high);

  uint8_t pin_;
  bool started_ = false;
  bool muted_ = false;
  bool outputHigh_ = false;
  BuzzerPattern pattern_ = BuzzerPattern::Off;
  uint32_t patternStartMs_ = 0;
};

}  // namespace aether

