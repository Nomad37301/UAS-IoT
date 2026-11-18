#include "RecoveryRelay.h"

#include <cassert>
#include <iostream>
#include <vector>

namespace {
bool configured = false;
std::vector<uint8_t> writes;
}

void pinMode(uint8_t pin, uint8_t mode) {
  assert(pin == 16 && mode == OUTPUT);
  configured = true;
}
void digitalWrite(uint8_t pin, uint8_t level) {
  assert(configured && pin == 16);
  assert(level == HIGH || level == LOW);
  writes.push_back(level);
}

int main() {
  using namespace aether;
  RecoveryRelay relay(16, RelayPolarity::ActiveHigh);
  assert(relay.trigger(0) == RelayTriggerResult::NotStarted);
  relay.cancel(0);
  relay.update(0);
  assert(writes.empty());
  relay.begin();
  assert(writes.size() == 1 && writes.back() == LOW && !relay.active());
  assert(relay.cooldownRemainingMs(0) == 0);

  assert(relay.trigger(1000) == RelayTriggerResult::Started);
  assert(relay.active() && writes.back() == HIGH);
  const auto countAfterTrigger = writes.size();
  assert(relay.trigger(1200) == RelayTriggerResult::Busy);
  relay.update(1499);
  assert(relay.active() && writes.size() == countAfterTrigger);

  // Permintaan pada batas pulsa harus melepas output dan ditolak sebagai cooldown.
  assert(relay.trigger(1500) == RelayTriggerResult::Cooldown);
  assert(!relay.active() && writes.back() == LOW);
  assert(relay.cooldownRemainingMs(1500) == 30000);
  const auto countAfterRelease = writes.size();
  assert(relay.trigger(31499) == RelayTriggerResult::Cooldown);
  assert(relay.cooldownRemainingMs(31499) == 1);
  assert(writes.size() == countAfterRelease);
  assert(relay.trigger(31500) == RelayTriggerResult::Started);
  assert(relay.active() && writes.back() == HIGH);

  relay.cancel(31510);
  assert(!relay.active() && writes.back() == LOW);
  assert(relay.cooldownRemainingMs(31510) == 30000);
  const auto countAfterCancel = writes.size();
  relay.cancel(32000);
  assert(writes.size() == countAfterCancel);
  assert(relay.cooldownRemainingMs(32000) == 29510);
  assert(relay.trigger(61509) == RelayTriggerResult::Cooldown);
  assert(relay.trigger(61510) == RelayTriggerResult::Started);

  // Loop terlambat: cooldown dihitung dari pelepasan aktual, bukan jadwal 500 ms.
  relay.update(62510);
  assert(!relay.active() && relay.cooldownRemainingMs(62510) == 30000);
  assert(relay.trigger(92509) == RelayTriggerResult::Cooldown);
  assert(relay.trigger(92510) == RelayTriggerResult::Started);

  // Reset/inisialisasi ulang mengembalikan OFF dan menghapus cooldown RAM.
  relay.begin();
  assert(!relay.active() && writes.back() == LOW);
  assert(relay.cooldownRemainingMs(0) == 0);

  // Pulsa melintasi overflow uint32_t.
  const uint32_t pulseStart = UINT32_MAX - 100U;
  assert(relay.trigger(pulseStart) == RelayTriggerResult::Started);
  relay.update(static_cast<uint32_t>(pulseStart + 499U));
  assert(relay.active());
  relay.update(static_cast<uint32_t>(pulseStart + 500U));
  assert(!relay.active() && writes.back() == LOW);
  assert(relay.cooldownRemainingMs(static_cast<uint32_t>(pulseStart + 500U)) == 30000);

  // Cooldown melintasi overflow secara terpisah.
  relay.begin();
  const uint32_t beforeWrap = UINT32_MAX - 600U;
  assert(relay.trigger(beforeWrap) == RelayTriggerResult::Started);
  const uint32_t releaseAt = beforeWrap + 500U;
  relay.update(releaseAt);
  assert(relay.trigger(static_cast<uint32_t>(releaseAt + 29999U)) == RelayTriggerResult::Cooldown);
  assert(relay.trigger(static_cast<uint32_t>(releaseAt + 30000U)) == RelayTriggerResult::Started);

  // Permintaan ditolak tidak diantrekan untuk dijalankan otomatis.
  relay.cancel(30000);
  assert(relay.trigger(30001) == RelayTriggerResult::Cooldown);
  relay.update(60000);
  assert(!relay.active() && relay.cooldownRemainingMs(60000) == 0);

  // Cabang generik aktif LOW diuji, tetapi sketch proyek tetap aktif HIGH.
  RecoveryRelay lowRelay(16, RelayPolarity::ActiveLow);
  lowRelay.begin();
  assert(writes.back() == HIGH && !lowRelay.active());
  assert(lowRelay.trigger(0) == RelayTriggerResult::Started);
  assert(writes.back() == LOW && lowRelay.active());
  lowRelay.update(500);
  assert(writes.back() == HIGH && !lowRelay.active());

  std::cout << "PASS: GPIO16, initial OFF, HIGH/LOW polarity, 500ms pulse, busy rejection, "
               "30s cooldown, cancel, delayed release, no queue, rollover\n";
}

