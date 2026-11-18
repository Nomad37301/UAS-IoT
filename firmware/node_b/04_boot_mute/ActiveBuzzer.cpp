#include "ActiveBuzzer.h"

namespace aether {

void ActiveBuzzer::begin() {
  pinMode(pin_, OUTPUT);
  digitalWrite(pin_, LOW);
  outputHigh_ = false;
  muted_ = false;
  pattern_ = BuzzerPattern::Off;
  patternStartMs_ = 0;
  started_ = true;
}

void ActiveBuzzer::writeOutput(bool high) {
  if (outputHigh_ == high) return;
  digitalWrite(pin_, high ? HIGH : LOW);
  outputHigh_ = high;
}

void ActiveBuzzer::setPattern(BuzzerPattern pattern, uint32_t nowMs) {
  if (!started_ || pattern_ == pattern) return;
  pattern_ = pattern;
  patternStartMs_ = nowMs;
  update(nowMs);  // Mulai atau hentikan output segera.
}

void ActiveBuzzer::setMuted(bool muted, uint32_t nowMs) {
  if (!started_ || muted_ == muted) return;
  if (muted) {
    // Aktifkan mute sebelum evaluasi fase agar tidak muncul pulsa ON sesaat.
    muted_ = true;
    update(nowMs);
    return;
  }
  // Evaluasi saat masih muted: Test yang kedaluwarsa tidak boleh diputar ulang.
  update(nowMs);
  muted_ = false;
  patternStartMs_ = nowMs;  // Unmute memulai fase bunyi baru.
  update(nowMs);
}

void ActiveBuzzer::update(uint32_t nowMs) {
  if (!started_) return;
  const uint32_t elapsed = static_cast<uint32_t>(nowMs - patternStartMs_);
  bool high = false;
  switch (pattern_) {
    case BuzzerPattern::Off:
      break;
    case BuzzerPattern::Test:
      if (elapsed >= 100U) {
        pattern_ = BuzzerPattern::Off;
      } else {
        high = true;
      }
      break;
    case BuzzerPattern::Warning:
      high = (elapsed % 1000U) < 150U;  // ON 150 ms, OFF 850 ms.
      break;
    case BuzzerPattern::Critical:
      high = (elapsed % 400U) < 200U;   // ON 200 ms, OFF 200 ms.
      break;
    default:
      pattern_ = BuzzerPattern::Off;  // Nilai tidak dikenal selalu diam.
      break;
  }
  writeOutput(high && !muted_);
}

const char* ActiveBuzzer::patternName(BuzzerPattern pattern) {
  switch (pattern) {
    case BuzzerPattern::Off: return "OFF";
    case BuzzerPattern::Test: return "TEST";
    case BuzzerPattern::Warning: return "WARNING";
    case BuzzerPattern::Critical: return "CRITICAL";
  }
  return "UNKNOWN";
}

}  // namespace aether

