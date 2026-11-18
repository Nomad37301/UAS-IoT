#pragma once

#include <stdint.h>

constexpr uint8_t INPUT = 1;
enum adc_attenuation_t { ADC_11db };

void pinMode(uint8_t pin, uint8_t mode);
void analogReadResolution(uint8_t bits);
void analogSetPinAttenuation(uint8_t pin, adc_attenuation_t attenuation);
uint16_t analogRead(uint8_t pin);
uint32_t analogReadMilliVolts(uint8_t pin);

