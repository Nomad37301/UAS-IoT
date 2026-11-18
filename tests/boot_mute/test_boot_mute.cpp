#include "ActiveBuzzer.h"
#include "DebouncedButton.h"

#include <cassert>
#include <iostream>

namespace {
int inputLevel = HIGH;
int outputLevel = HIGH;
bool inputConfigured = false;
bool outputConfigured = false;
unsigned reads = 0;
}  // namespace

void pinMode(uint8_t pin, uint8_t mode) {
  if (pin == 0) {
    assert(mode == INPUT_PULLUP);
    inputConfigured = true;
  } else {
    assert(pin == 15 && mode == OUTPUT);
    outputConfigured = true;
  }
}
int digitalRead(uint8_t pin) {
  assert(pin == 0 && inputConfigured);
  ++reads;
  return inputLevel;
}
void digitalWrite(uint8_t pin, uint8_t level) {
  assert(pin == 15 && outputConfigured);
  outputLevel = level;
}

int main() {
  using aether::ActiveBuzzer;
  using aether::BuzzerPattern;
  using aether::DebouncedButton;
  DebouncedButton button(0);
  assert(!button.update(0) && reads == 0);
  button.begin(0);
  assert(!button.armed() && !button.pressed());
  assert(!button.update(29) && !button.armed());
  assert(!button.update(30) && button.armed());

  // Pulsa <30 ms diabaikan, termasuk perubahan raw yang berulang.
  inputLevel = LOW;
  assert(!button.update(40));
  inputLevel = HIGH;
  assert(!button.update(55));
  assert(!button.update(85));
  assert(button.armed() && !button.pressed());
  inputLevel = LOW;
  assert(!button.update(100));
  assert(!button.update(129));
  assert(button.update(130));
  assert(button.pressed() && !button.armed());
  assert(!button.update(199));

  // Bounce pelepasan tidak boleh re-arm sebelum HIGH stabil 30 ms.
  inputLevel = HIGH;
  assert(!button.update(200));
  inputLevel = LOW;
  assert(!button.update(210));
  inputLevel = HIGH;
  assert(!button.update(220));
  assert(!button.update(249) && !button.armed());
  assert(!button.update(250) && button.armed());
  inputLevel = LOW;
  assert(!button.update(260));
  assert(button.update(290));
  assert(!button.update(1000));  // Tahan lama: tidak mengulang event.

  // Input awal LOW tidak menghasilkan event, sampai lepas lalu tekan.
  DebouncedButton heldAtStartup(0);
  heldAtStartup.begin(1000);
  assert(!heldAtStartup.update(1030));
  assert(heldAtStartup.pressed() && !heldAtStartup.armed());
  assert(!heldAtStartup.update(2000));
  inputLevel = HIGH;
  assert(!heldAtStartup.update(2100));
  assert(!heldAtStartup.update(2130) && heldAtStartup.armed());
  inputLevel = LOW;
  assert(!heldAtStartup.update(2200));
  assert(heldAtStartup.update(2230));

  // Debounce tetap tepat melintasi overflow millis().
  inputLevel = HIGH;
  const uint32_t start = UINT32_MAX - 40U;
  button.begin(start);
  assert(!button.update(start + 30U) && button.armed());
  inputLevel = LOW;
  assert(!button.update(start + 35U));
  assert(!button.update(static_cast<uint32_t>(start + 64U)));
  assert(button.update(static_cast<uint32_t>(start + 65U)));

  // Integrasi driver asli: bounce -> satu mute, tahan -> tidak mengulang,
  // unmute manual -> lepas/tekan berikutnya mematikan bunyi kembali.
  inputLevel = HIGH;
  button.begin(0);
  ActiveBuzzer buzzer(15);
  buzzer.begin();
  unsigned pressCount = 0;
  auto service = [&](uint32_t now) {
    if (button.update(now)) {
      ++pressCount;
      buzzer.setMuted(true, now);
    }
    buzzer.update(now);
  };
  service(30);
  buzzer.setPattern(BuzzerPattern::Critical, 100);
  assert(outputLevel == HIGH);
  inputLevel = LOW;
  service(120);
  inputLevel = HIGH;
  service(125);
  inputLevel = LOW;
  service(130);
  service(159);
  assert(pressCount == 0 && outputLevel == HIGH);
  service(160);
  assert(pressCount == 1 && buzzer.muted() && outputLevel == LOW);
  assert(buzzer.pattern() == BuzzerPattern::Critical);
  service(500);
  assert(pressCount == 1 && outputLevel == LOW);
  inputLevel = HIGH;
  service(501);
  service(531);
  buzzer.setMuted(false, 540);
  assert(outputLevel == HIGH);
  inputLevel = LOW;
  service(550);
  service(580);
  assert(pressCount == 2 && buzzer.muted() && outputLevel == LOW);

  // Tekan lagi saat muted: tidak boleh toggle menjadi bunyi.
  inputLevel = HIGH;
  service(600);
  service(630);
  inputLevel = LOW;
  service(640);
  service(670);
  assert(pressCount == 3 && buzzer.muted() && outputLevel == LOW);
  std::cout << "PASS: pull-up, press/release debounce, startup held, one-shot press, "
               "rollover, button-to-buzzer mute, idempotence\n";
}

