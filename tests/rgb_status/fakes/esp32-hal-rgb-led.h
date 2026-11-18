#pragma once

#include <stdint.h>

enum rgb_led_color_order_t { LED_COLOR_ORDER_GRB };
void rgbLedWriteOrdered(uint8_t pin, rgb_led_color_order_t order,
                        uint8_t red, uint8_t green, uint8_t blue);

