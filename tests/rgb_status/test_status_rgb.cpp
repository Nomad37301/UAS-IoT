#include "StatusRgb.h"
#include <esp32-hal-rgb-led.h>

#include <cassert>
#include <iostream>
#include <vector>

namespace {
struct Color { uint8_t red, green, blue; };
std::vector<Color> frames;

void expectColor(const aether::StatusRgb& led, uint8_t red, uint8_t green, uint8_t blue) {
  assert(!frames.empty());
  assert(frames.back().red == red && frames.back().green == green && frames.back().blue == blue);
  assert(led.red() == red && led.green() == green && led.blue() == blue);
}
}  // namespace

void rgbLedWriteOrdered(uint8_t pin, rgb_led_color_order_t order,
                        uint8_t red, uint8_t green, uint8_t blue) {
  assert(pin == 48 && order == LED_COLOR_ORDER_GRB);
  frames.push_back({red, green, blue});
}

int main() {
  using namespace aether;
  static_assert(STATE_HEALTHY == 0 && STATE_EXT_ISOLATED == 1 &&
                STATE_SERVICE_DOWN == 2 && STATE_NETWORK_DEAD == 3 &&
                STATE_HW_ANOMALY == 4, "Kode state harus sesuai kontrak");
  StatusRgb led(48);
  assert(!led.setState(STATE_HEALTHY, 0));
  led.startTest(0);
  led.off(0);
  led.update(0);
  assert(frames.empty());
  led.begin();
  expectColor(led, 0, 0, 0);
  assert(led.mode() == RgbMode::Off);
  assert(led.setState(STATE_HEALTHY, 10));
  expectColor(led, 0, 32, 0);
  assert(led.setState(STATE_EXT_ISOLATED, 20));
  expectColor(led, 32, 32, 0);
  assert(led.setState(STATE_SERVICE_DOWN, 30));
  expectColor(led, 32, 0, 0);

  // Perubahan state yang sama tidak mengulang fase atau menulis frame lagi.
  assert(led.setState(STATE_NETWORK_DEAD, 100));
  led.update(599);
  expectColor(led, 32, 32, 0);
  led.update(600);
  expectColor(led, 32, 0, 0);
  const auto previousCount = frames.size();
  assert(led.setState(STATE_NETWORK_DEAD, 650));
  led.update(1099);
  assert(frames.size() == previousCount);
  expectColor(led, 32, 0, 0);
  led.update(1100);
  expectColor(led, 32, 32, 0);

  led.setState(STATE_HW_ANOMALY, 1200);
  led.update(1349);
  expectColor(led, 32, 0, 0);
  led.update(1350);
  expectColor(led, 0, 0, 0);
  led.update(1499);
  expectColor(led, 0, 0, 0);
  led.update(1500);
  expectColor(led, 32, 0, 0);

  led.startTest(2000);
  expectColor(led, 32, 0, 0);
  led.update(2299);
  expectColor(led, 32, 0, 0);
  led.update(2300);
  expectColor(led, 0, 32, 0);
  led.update(2599);
  expectColor(led, 0, 32, 0);
  led.update(2600);
  expectColor(led, 0, 0, 32);
  led.update(2899);
  expectColor(led, 0, 0, 32);
  led.update(2900);
  expectColor(led, 0, 0, 0);
  assert(led.mode() == RgbMode::Off);

  // Test baru mengulang dari merah; loop terlambat langsung selesai ke OFF.
  led.startTest(3000);
  led.update(3600);
  expectColor(led, 0, 0, 32);
  led.startTest(3700);
  expectColor(led, 32, 0, 0);
  led.update(6000);
  assert(led.mode() == RgbMode::Off);
  expectColor(led, 0, 0, 0);

  const uint32_t start = UINT32_MAX - 250U;
  led.setState(STATE_NETWORK_DEAD, start);
  led.update(static_cast<uint32_t>(start + 499U));
  expectColor(led, 32, 32, 0);
  led.update(static_cast<uint32_t>(start + 500U));
  expectColor(led, 32, 0, 0);
  led.update(static_cast<uint32_t>(start + 1000U));
  expectColor(led, 32, 32, 0);
  led.startTest(start);
  led.update(static_cast<uint32_t>(start + 900U));
  assert(led.mode() == RgbMode::Off);
  expectColor(led, 0, 0, 0);

  led.setState(STATE_HEALTHY, 1000);
  assert(!led.setState(static_cast<AetherState>(255), 1001));
  assert(led.mode() == RgbMode::Off);
  expectColor(led, 0, 0, 0);
  led.setState(STATE_HEALTHY, 1002);
  led.off(1003);
  expectColor(led, 0, 0, 0);

  StatusRgb dim(48, 8);
  dim.begin();
  dim.setState(STATE_EXT_ISOLATED, 0);
  expectColor(dim, 8, 8, 0);
  std::cout << "PASS: GPIO48/GRB, state codes, colors, phase boundaries, repeated state, "
               "finite test, delayed update, rollover, invalid state, brightness\n";
}

