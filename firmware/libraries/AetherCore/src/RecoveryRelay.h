#pragma once

#include <Arduino.h>

namespace aether {

enum class RelayPolarity : uint8_t { ActiveLow, ActiveHigh };
enum class RelayTriggerResult : uint8_t { Started, NotStarted, Busy, Cooldown };

constexpr uint32_t kRecoveryPulseMs = 500;
constexpr uint32_t kRecoveryCooldownMs = 30000;

// Gunakan begin(), trigger(), cancel(), update() dari satu loop pemilik.
class RecoveryRelay {
 public:
  // Polaritas wajib dipilih eksplisit, tidak memakai default tersembunyi.
  RecoveryRelay(uint8_t pin, RelayPolarity polarity) : pin_(pin), polarity_(polarity) {}

  void begin();
  RelayTriggerResult trigger(uint32_t nowMs);
  void cancel(uint32_t nowMs);
  void update(uint32_t nowMs);

  bool active() const { return active_; }
  uint32_t cooldownRemainingMs(uint32_t nowMs) const;
  static const char* resultName(RelayTriggerResult result);

 private:
  void drive(bool active);
  void release(uint32_t nowMs);

  uint8_t pin_;
  RelayPolarity polarity_;
  bool started_ = false;
  bool active_ = false;
  bool coolingDown_ = false;
  uint32_t pulseStartMs_ = 0;
  uint32_t releasedMs_ = 0;
};

}  // namespace aether

