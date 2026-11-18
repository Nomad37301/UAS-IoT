#pragma once
#if __has_include("Secrets.h")
#include "Secrets.h"
#endif
#ifndef AETHER_WIFI_SSID
#define AETHER_WIFI_SSID ""
#endif
#ifndef AETHER_WIFI_PASSWORD
#define AETHER_WIFI_PASSWORD ""
#endif
#ifndef AETHER_PI_HOST
#define AETHER_PI_HOST "192.168.1.10"
#endif
#ifndef AETHER_MQTT_USER
#define AETHER_MQTT_USER ""
#endif
#ifndef AETHER_MQTT_PASSWORD
#define AETHER_MQTT_PASSWORD ""
#endif
#ifndef AETHER_IR_LIMIT_C
#define AETHER_IR_LIMIT_C 70.0f
#endif
#ifndef AETHER_AUTO_RECOVERY
#define AETHER_AUTO_RECOVERY true
#endif
