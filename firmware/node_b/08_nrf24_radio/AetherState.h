#pragma once

#include <stdint.h>

namespace aether {

// Nilai persis kontrak AetherState; mode OFF/TEST tidak menambah kode diagnosis.
enum AetherState : uint8_t {
  STATE_HEALTHY = 0,
  STATE_EXT_ISOLATED = 1,
  STATE_SERVICE_DOWN = 2,
  STATE_NETWORK_DEAD = 3,
  STATE_HW_ANOMALY = 4,
};

}  // namespace aether

