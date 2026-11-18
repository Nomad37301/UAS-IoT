#include "DebouncedButton.h"

namespace aether {
namespace {
constexpr uint32_t kDebounceMs = 30;
}

void DebouncedButton::begin(uint32_t nowMs) {
  pinMode(pin_, INPUT_PULLUP);
  candidatePressed_ = digitalRead(pin_) == LOW;
  stablePressed_ = candidatePressed_;
  candidateSinceMs_ = nowMs;
  settled_ = false;
  armed_ = false;  // Harus melihat kondisi lepas stabil lebih dahulu.
  started_ = true;
}

bool DebouncedButton::update(uint32_t nowMs) {
  if (!started_) return false;
  const bool rawPressed = digitalRead(pin_) == LOW;
  if (rawPressed != candidatePressed_) {
    candidatePressed_ = rawPressed;
    candidateSinceMs_ = nowMs;
  }
  if (static_cast<uint32_t>(nowMs - candidateSinceMs_) < kDebounceMs) return false;
  if (settled_ && stablePressed_ == candidatePressed_) return false;

  stablePressed_ = candidatePressed_;
  settled_ = true;
  if (!stablePressed_) {
    armed_ = true;
    return false;
  }
  if (!armed_) return false;
  armed_ = false;
  return true;
}

}  // namespace aether

