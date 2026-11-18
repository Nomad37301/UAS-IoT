#pragma once

#include <Arduino.h>
#include "AetherState.h"

namespace aether {

enum class RgbMode : uint8_t { Off, State, Test };

class StatusRgb {
 public:
  explicit StatusRgb(uint8_t pin, uint8_t brightness = 32)
      : pin_(pin), brightness_(brightness) {}

  void begin();
  bool setState(AetherState state, uint32_t nowMs);
  void off(uint32_t nowMs);
  void startTest(uint32_t nowMs);
  void update(uint32_t nowMs);

  RgbMode mode() const { return mode_; }
  AetherState state() const { return state_; }  // Bermakna hanya saat mode State.
  uint8_t red() const { return red_; }
  uint8_t green() const { return green_; }
  uint8_t blue() const { return blue_; }
  static const char* stateName(AetherState state);
  static const char* modeName(RgbMode mode);

 private:
  void show(uint8_t red, uint8_t green, uint8_t blue);

  uint8_t pin_;
  uint8_t brightness_;
  bool started_ = false;
  RgbMode mode_ = RgbMode::Off;
  AetherState state_ = STATE_HEALTHY;
  uint32_t phaseStartMs_ = 0;
  uint8_t red_ = 0;
  uint8_t green_ = 0;
  uint8_t blue_ = 0;
};

}  // namespace aether

