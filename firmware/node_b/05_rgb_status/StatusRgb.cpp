#include "StatusRgb.h"
#include <esp32-hal-rgb-led.h>

namespace aether {

void StatusRgb::begin() {
  // WS2812 menerima frame serial RGB, bukan digitalWrite HIGH/LOW biasa.
  rgbLedWriteOrdered(pin_, LED_COLOR_ORDER_GRB, 0, 0, 0);
  red_ = green_ = blue_ = 0;
  mode_ = RgbMode::Off;
  state_ = STATE_HEALTHY;
  phaseStartMs_ = 0;
  started_ = true;
}

void StatusRgb::show(uint8_t red, uint8_t green, uint8_t blue) {
  if (red == red_ && green == green_ && blue == blue_) return;
  rgbLedWriteOrdered(pin_, LED_COLOR_ORDER_GRB, red, green, blue);
  red_ = red;
  green_ = green;
  blue_ = blue;
}

bool StatusRgb::setState(AetherState state, uint32_t nowMs) {
  if (!started_) return false;
  if (static_cast<uint8_t>(state) > STATE_HW_ANOMALY) {
    off(nowMs);
    return false;
  }
  if (mode_ == RgbMode::State && state_ == state) return true;
  state_ = state;
  mode_ = RgbMode::State;
  phaseStartMs_ = nowMs;
  update(nowMs);
  return true;
}

void StatusRgb::off(uint32_t nowMs) {
  if (!started_) return;
  mode_ = RgbMode::Off;
  update(nowMs);
}

void StatusRgb::startTest(uint32_t nowMs) {
  if (!started_) return;
  mode_ = RgbMode::Test;
  phaseStartMs_ = nowMs;
  update(nowMs);
}

void StatusRgb::update(uint32_t nowMs) {
  if (!started_) return;
  const uint32_t elapsed = static_cast<uint32_t>(nowMs - phaseStartMs_);
  uint8_t red = 0, green = 0, blue = 0;
  if (mode_ == RgbMode::Test) {
    if (elapsed < 300U) red = brightness_;
    else if (elapsed < 600U) green = brightness_;
    else if (elapsed < 900U) blue = brightness_;
    else mode_ = RgbMode::Off;
  } else if (mode_ == RgbMode::State) {
    switch (state_) {
      case STATE_HEALTHY: green = brightness_; break;
      case STATE_EXT_ISOLATED: red = green = brightness_; break;
      case STATE_SERVICE_DOWN: red = brightness_; break;
      case STATE_NETWORK_DEAD:
        red = brightness_;
        if ((elapsed % 1000U) < 500U) green = brightness_;
        break;
      case STATE_HW_ANOMALY:
        if ((elapsed % 300U) < 150U) red = brightness_;
        break;
    }
  }
  show(red, green, blue);
}

const char* StatusRgb::stateName(AetherState state) {
  switch (state) {
    case STATE_HEALTHY: return "HEALTHY";
    case STATE_EXT_ISOLATED: return "EXT_ISOLATED";
    case STATE_SERVICE_DOWN: return "SERVICE_DOWN";
    case STATE_NETWORK_DEAD: return "NETWORK_DEAD";
    case STATE_HW_ANOMALY: return "HW_ANOMALY";
  }
  return "UNKNOWN";
}

const char* StatusRgb::modeName(RgbMode mode) {
  switch (mode) {
    case RgbMode::Off: return "OFF";
    case RgbMode::State: return "STATE";
    case RgbMode::Test: return "TEST";
  }
  return "UNKNOWN";
}

}  // namespace aether

