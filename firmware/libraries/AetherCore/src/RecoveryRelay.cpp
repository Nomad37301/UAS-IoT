#include "RecoveryRelay.h"

namespace aether {

void RecoveryRelay::drive(bool active) {
  const bool high = (polarity_ == RelayPolarity::ActiveHigh) ? active : !active;
  digitalWrite(pin_, high ? HIGH : LOW);
  active_ = active;
}

void RecoveryRelay::begin() {
  pinMode(pin_, OUTPUT);
  drive(false);
  coolingDown_ = false;
  pulseStartMs_ = releasedMs_ = 0;
  started_ = true;
}

void RecoveryRelay::release(uint32_t nowMs) {
  drive(false);
  coolingDown_ = true;
  releasedMs_ = nowMs;  // Jeda dihitung setelah output benar-benar diperintahkan OFF.
}

void RecoveryRelay::update(uint32_t nowMs) {
  if (!started_) return;
  if (active_ && static_cast<uint32_t>(nowMs - pulseStartMs_) >= kRecoveryPulseMs) {
    release(nowMs);
  }
  if (coolingDown_ && static_cast<uint32_t>(nowMs - releasedMs_) >= kRecoveryCooldownMs) {
    coolingDown_ = false;
  }
}

RelayTriggerResult RecoveryRelay::trigger(uint32_t nowMs) {
  if (!started_) return RelayTriggerResult::NotStarted;
  update(nowMs);
  if (active_) return RelayTriggerResult::Busy;
  if (coolingDown_) return RelayTriggerResult::Cooldown;
  pulseStartMs_ = nowMs;
  drive(true);
  return RelayTriggerResult::Started;
}

void RecoveryRelay::cancel(uint32_t nowMs) {
  if (!started_) return;
  // Batalkan langsung tanpa menyalakan output atau mengulang timer cooldown.
  if (active_) release(nowMs);
  update(nowMs);
}

uint32_t RecoveryRelay::cooldownRemainingMs(uint32_t nowMs) const {
  if (!coolingDown_) return 0;
  const uint32_t elapsed = static_cast<uint32_t>(nowMs - releasedMs_);
  return elapsed >= kRecoveryCooldownMs ? 0 : kRecoveryCooldownMs - elapsed;
}

const char* RecoveryRelay::resultName(RelayTriggerResult result) {
  switch (result) {
    case RelayTriggerResult::Started: return "STARTED";
    case RelayTriggerResult::NotStarted: return "NOT_STARTED";
    case RelayTriggerResult::Busy: return "BUSY";
    case RelayTriggerResult::Cooldown: return "COOLDOWN";
  }
  return "UNKNOWN";
}

}  // namespace aether

