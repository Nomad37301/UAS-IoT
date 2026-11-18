#include "ActiveBuzzer.h"

#include <cassert>
#include <iostream>
#include <vector>

namespace {
bool configured = false;
std::vector<uint8_t> writes;

void expectOutput(const aether::ActiveBuzzer& buzzer, bool high) {
  assert(buzzer.outputHigh() == high);
  assert(!writes.empty() && writes.back() == (high ? HIGH : LOW));
}
}  // namespace

void pinMode(uint8_t pin, uint8_t mode) {
  assert(pin == 15 && mode == OUTPUT);
  configured = true;
}
void digitalWrite(uint8_t pin, uint8_t level) {
  assert(configured && pin == 15);
  assert(level == HIGH || level == LOW);
  writes.push_back(level);
}

int main() {
  using aether::ActiveBuzzer;
  using aether::BuzzerPattern;
  ActiveBuzzer buzzer(15);
  buzzer.setPattern(BuzzerPattern::Critical, 0);
  buzzer.setMuted(true, 0);
  buzzer.update(0);
  assert(writes.empty());  // Belum begin: tidak boleh menyentuh pin.
  buzzer.begin();
  assert(writes.size() == 1 && !buzzer.muted());
  expectOutput(buzzer, false);

  buzzer.setPattern(BuzzerPattern::Warning, 1000);
  expectOutput(buzzer, true);
  const auto initialWrites = writes.size();
  buzzer.update(1149);
  assert(writes.size() == initialWrites);
  buzzer.update(1150);
  expectOutput(buzzer, false);
  buzzer.update(1999);
  expectOutput(buzzer, false);
  buzzer.update(2000);
  expectOutput(buzzer, true);
  buzzer.setPattern(BuzzerPattern::Warning, 2050);
  buzzer.update(2150);
  expectOutput(buzzer, false);  // Pola sama tidak mengulang fase.

  buzzer.setPattern(BuzzerPattern::Critical, 2200);
  expectOutput(buzzer, true);
  buzzer.update(2399);
  expectOutput(buzzer, true);
  buzzer.update(2400);
  expectOutput(buzzer, false);
  buzzer.update(2600);
  expectOutput(buzzer, true);
  buzzer.setMuted(true, 2601);
  expectOutput(buzzer, false);
  assert(buzzer.pattern() == BuzzerPattern::Critical);
  buzzer.setPattern(BuzzerPattern::Warning, 2700);
  expectOutput(buzzer, false);
  buzzer.setMuted(false, 2800);
  expectOutput(buzzer, true);
  buzzer.update(2950);
  expectOutput(buzzer, false);

  buzzer.setPattern(BuzzerPattern::Test, 3000);
  expectOutput(buzzer, true);
  buzzer.update(3099);
  expectOutput(buzzer, true);
  buzzer.update(3100);
  expectOutput(buzzer, false);
  assert(buzzer.pattern() == BuzzerPattern::Off);

  buzzer.setMuted(true, 4000);
  buzzer.setPattern(BuzzerPattern::Test, 4000);
  expectOutput(buzzer, false);
  // Unmute harus menyelesaikan Test kedaluwarsa meski update sempat terlambat.
  buzzer.setMuted(false, 4200);
  assert(buzzer.pattern() == BuzzerPattern::Off);
  expectOutput(buzzer, false);

  buzzer.setPattern(BuzzerPattern::Critical, 5000);
  buzzer.update(5200);
  expectOutput(buzzer, false);
  const auto beforeMute = writes.size();
  buzzer.setMuted(true, 5400);  // Fase berikutnya ON, tetapi perintah ini MUTE.
  assert(writes.size() == beforeMute);  // Jangan menghasilkan pulsa HIGH sementara.
  expectOutput(buzzer, false);
  buzzer.setPattern(BuzzerPattern::Off, 5401);
  assert(buzzer.muted());  // Stop tidak menghapus latch mute.

  buzzer.begin();
  assert(!buzzer.muted() && buzzer.pattern() == BuzzerPattern::Off);
  const uint32_t rolloverStart = UINT32_MAX - 50U;
  buzzer.setPattern(BuzzerPattern::Test, rolloverStart);
  buzzer.update(static_cast<uint32_t>(rolloverStart + 99U));
  expectOutput(buzzer, true);
  buzzer.update(static_cast<uint32_t>(rolloverStart + 100U));
  expectOutput(buzzer, false);
  buzzer.setPattern(BuzzerPattern::Critical, rolloverStart);
  buzzer.update(static_cast<uint32_t>(rolloverStart + 200U));
  expectOutput(buzzer, false);
  buzzer.update(static_cast<uint32_t>(rolloverStart + 400U));
  expectOutput(buzzer, true);

  // Loop terlambat: pilih fase sekarang, bukan mengejar beep yang terlewat.
  buzzer.update(static_cast<uint32_t>(rolloverStart + 10000U));
  expectOutput(buzzer, true);
  buzzer.setPattern(static_cast<BuzzerPattern>(255), 10001);
  assert(buzzer.pattern() == BuzzerPattern::Off);
  expectOutput(buzzer, false);

  std::cout << "PASS: silent init, pattern timing, mute, expiry, delayed update, "
               "no duplicate writes, rollover, unknown pattern\n";
}

