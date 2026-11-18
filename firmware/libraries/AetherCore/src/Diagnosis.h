#pragma once
#include "AetherState.h"

namespace aether {
struct Evidence {
  bool voltageValid = false, sensorValid = false, modbusOk = false;
  bool wifi = false, mqtt = false, httpObserved = false, externalFresh = false;
  bool externalRadioAlive = false;
  unsigned http = 0, externalHttp = 0;
  float voltage = 0, ir = 0, maxIr = 70;
};
struct Diagnosis { AetherState state; const char* reason; };
inline Diagnosis diagnose(const Evidence& e) {
  if (e.voltageValid && e.voltage < 4.5f) return {STATE_HW_ANOMALY, "POWER_FAULT"};
  if (e.sensorValid && e.ir >= e.maxIr) return {STATE_HW_ANOMALY, "THERMAL_FAULT"};
  if (!e.httpObserved) return {STATE_NETWORK_DEAD, "WAITING_FOR_PROBES"};
  if (!e.sensorValid || !e.voltageValid) return {STATE_HW_ANOMALY, "SENSOR_UNAVAILABLE"};
  if (!e.modbusOk && e.http != 200) return {STATE_NETWORK_DEAD, "FULL_HOST_DOWN_SUSPECTED"};
  if (!e.wifi || !e.mqtt) return {STATE_NETWORK_DEAD, "NETWORK_OR_BROKER_DOWN"};
  if (e.http != 200 && e.modbusOk) return {STATE_SERVICE_DOWN, "SERVICE_DOWN_HOST_ALIVE"};
  if (!e.modbusOk) return {STATE_NETWORK_DEAD, "UART_UNAVAILABLE"};
  if (!e.externalFresh) return {STATE_EXT_ISOLATED, "EXTERNAL_PROBE_STALE"};
  if (e.externalFresh && e.externalHttp != 200) return {STATE_EXT_ISOLATED, "EXTERNAL_NETWORK_ISOLATION"};
  if (!e.externalRadioAlive) return {STATE_NETWORK_DEAD, "RADIO_LINK_UNAVAILABLE"};
  return {STATE_HEALTHY, "HEALTHY"};
}
}  // namespace aether
