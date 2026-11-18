#pragma once

#include <Arduino.h>

// Beberapa varian core sudah mendefinisikan nama ini; kontrak proyek tetap 48.
#ifndef PIN_RGB_LED
#define PIN_RGB_LED 48
#endif
#if PIN_RGB_LED != 48
#error "AETHER Node B memerlukan RGB LED GPIO 48 sesuai kontrak proyek."
#endif

