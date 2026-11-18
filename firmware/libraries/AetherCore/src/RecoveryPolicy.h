#pragma once
#include "AetherState.h"
#include <stdint.h>

namespace aether {
enum class RecoveryAction : uint8_t { None, SerialRestart, RelayPulse };
// Pure scheduling policy. Caller commits only actions actually started.
class RecoveryPolicy {
 public:
  RecoveryAction evaluate(uint32_t now, AetherState state, bool serialReady, bool relayReady) {
    if (now >= 30000) bootReady_ = true;
    if (state == STATE_HEALTHY) attempts_ = 0;
    if (state != STATE_SERVICE_DOWN) { eligible_ = false; stage_ = 0; return RecoveryAction::None; }
    if (!eligible_) { eligible_ = true; faultSince_ = now; stage_ = 0; }
    if (!bootReady_ || static_cast<uint32_t>(now - faultSince_) < 10000) return RecoveryAction::None;
    if (stage_ == 0 && attempts_ < 3 && serialReady) return RecoveryAction::SerialRestart;
    // Pi daemon throttles requests for 30 s; fallback must start after that window.
    if (stage_ == 1 && static_cast<uint32_t>(now - actionAt_) >= 35000 && relayReady) return RecoveryAction::RelayPulse;
    if (stage_ == 2 && static_cast<uint32_t>(now - actionAt_) >= 60000) stage_ = 0;
    return RecoveryAction::None;
  }
  void commit(RecoveryAction action, uint32_t now) {
    if (action == RecoveryAction::SerialRestart && stage_ == 0 && attempts_ < 3) {
      ++attempts_; stage_ = 1; actionAt_ = now;
    } else if (action == RecoveryAction::RelayPulse && stage_ == 1) { stage_ = 2; actionAt_ = now; }
  }
  unsigned attempts() const { return attempts_; }
 private:
  bool bootReady_ = false, eligible_ = false;
  unsigned stage_ = 0, attempts_ = 0;
  uint32_t faultSince_ = 0, actionAt_ = 0;
};
}  // namespace aether
