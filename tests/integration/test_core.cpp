#include "Diagnosis.h"
#include "RecoveryPolicy.h"
#include "CoapCodec.h"
#include <cassert>
#include <fstream>
#include <iostream>

int main() {
  using namespace aether;
  Evidence e;
  assert(diagnose(e).state == STATE_NETWORK_DEAD);
  e.voltageValid = e.sensorValid = e.modbusOk = e.wifi = e.mqtt = e.httpObserved = true;
  e.voltage = 5.0f; e.ir = 35; e.http = 200;
  e.externalFresh = true; e.externalHttp = 200;
  e.externalRadioAlive = true;
  assert(diagnose(e).state == STATE_HEALTHY);
  e.externalRadioAlive = false;
  assert(std::string(diagnose(e).reason) == "RADIO_LINK_UNAVAILABLE");
  e.externalRadioAlive = true;
  e.externalFresh = false;
  assert(std::string(diagnose(e).reason) == "EXTERNAL_PROBE_STALE");
  e.externalFresh = true; e.externalHttp = 403;
  assert(diagnose(e).state == STATE_EXT_ISOLATED);
  e.externalFresh = false; e.http = 503;
  assert(diagnose(e).state == STATE_SERVICE_DOWN);
  e.mqtt = false;
  assert(diagnose(e).state == STATE_NETWORK_DEAD);
  e.modbusOk = false;
  assert(std::string(diagnose(e).reason) == "FULL_HOST_DOWN_SUSPECTED");
  e.voltage = 4.49f;
  assert(diagnose(e).state == STATE_HW_ANOMALY);
  e.voltage = 5; e.ir = 70;
  assert(std::string(diagnose(e).reason) == "THERMAL_FAULT");
  e.ir = 30; e.sensorValid = false;
  assert(std::string(diagnose(e).reason) == "SENSOR_UNAVAILABLE");

  RecoveryPolicy policy;
  assert(policy.evaluate(0, STATE_SERVICE_DOWN, true, true) == RecoveryAction::None);
  assert(policy.evaluate(29999, STATE_SERVICE_DOWN, true, true) == RecoveryAction::None);
  uint32_t now = 30000;
  for (unsigned attempt = 1; attempt <= 3; ++attempt) {
    assert(policy.evaluate(now, STATE_SERVICE_DOWN, false, true) == RecoveryAction::None);
    assert(policy.evaluate(now, STATE_SERVICE_DOWN, true, true) == RecoveryAction::SerialRestart);
    policy.commit(RecoveryAction::SerialRestart, now);
    assert(policy.attempts() == attempt);
    assert(policy.evaluate(now + 34999, STATE_SERVICE_DOWN, true, true) == RecoveryAction::None);
    assert(policy.evaluate(now + 35000, STATE_SERVICE_DOWN, true, false) == RecoveryAction::None);
    assert(policy.evaluate(now + 35000, STATE_SERVICE_DOWN, true, true) == RecoveryAction::RelayPulse);
    policy.commit(RecoveryAction::RelayPulse, now + 35000);
    assert(policy.evaluate(now + 94999, STATE_SERVICE_DOWN, true, true) == RecoveryAction::None);
    policy.evaluate(now + 95000, STATE_SERVICE_DOWN, true, true);
    now += 95001;
  }
  assert(policy.evaluate(now, STATE_SERVICE_DOWN, true, true) == RecoveryAction::None);
  assert(policy.evaluate(now + 1, STATE_HW_ANOMALY, true, true) == RecoveryAction::None);
  assert(policy.evaluate(now + 100000, STATE_NETWORK_DEAD, true, true) == RecoveryAction::None);
  policy.evaluate(now + 100001, STATE_HEALTHY, true, true);
  assert(policy.attempts() == 0);
  uint32_t start = UINT32_MAX - 5000;
  assert(policy.evaluate(start, STATE_SERVICE_DOWN, true, true) == RecoveryAction::None);
  assert(policy.evaluate(uint32_t(start + 10000U), STATE_SERVICE_DOWN, true, true) == RecoveryAction::SerialRestart);

  const uint8_t get[] = {0x42, 1, 0x12, 0x34, 'A','B', 0xb4,'3','3','0','3',0x01,'0',0x04,'5','7','0','0'};
  CoapMessage request;
  assert(parseCoap(get, sizeof(get), request));
  assert(!strcmp(request.path, "/3303/0/5700") && request.mid == 0x1234);
  uint8_t encoded[256];
  const auto size = coapResponse(request, 69, "37.50", false, encoded, sizeof(encoded));
  CoapMessage reply;
  assert(parseCoap(encoded, size, reply) && reply.type == 2 && reply.code == 69);
  assert(reply.payloadLength == 5 && !memcmp(reply.payload, "37.50", 5));
  const uint8_t invalid[] = {0x40, 1, 0, 0, 0xff};
  assert(!parseCoap(invalid, sizeof(invalid), reply));
  const uint8_t critical[] = {0x40, 1, 0, 0, 0x50};
  assert(parseCoap(critical, sizeof(critical), reply) && reply.unsupportedOption);
  const char* body = "{\"system\":\"AETHER\",\"node\":\"nodeB\",\"state\":\"HEALTHY\"}";
  const auto reportSize = coapReport(0x1234, body, encoded, sizeof(encoded));
  assert(parseCoap(encoded, reportSize, reply) && !strcmp(reply.path, "/aether/nodeB"));
  std::ofstream file(".build/coap-report.bin", std::ios::binary);
  file.write(reinterpret_cast<char*>(encoded), reportSize); file.close();
  uint32_t seed = 1;
  uint8_t fuzz[256];
  for (unsigned round = 0; round < 10000; ++round) {
    for (auto& byte : fuzz) { seed = seed * 1664525U + 1013904223U; byte = seed >> 24; }
    parseCoap(fuzz, round % sizeof(fuzz), reply);
  }
  std::cout << "PASS: diagnosis matrix, sustained/limited recovery, third fallback, rollover, CoAP codec/fuzz\n";
}
