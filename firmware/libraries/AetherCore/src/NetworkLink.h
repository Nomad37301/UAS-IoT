#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace aether {
enum class NetAction : uint8_t { Recovery, Mute };
struct NetCommand { NetAction action; char requestId[48]; };
struct NetStatus {
  bool wifi = false, mqtt = false, coap = false, demoOverheat = false;
  bool externalRadioAlive = false;
  uint16_t http = 0, externalHttp = 0;
  uint32_t latency = 0, probeAt = 0, externalAt = 0, demoAt = 0;
};
struct NetPayload { char text[1536] = {}; };
struct NetworkSettings {
  const char* ssid; const char* password; const char* host;
  const char* mqttUser; const char* mqttPassword; const char* node;
  bool nodeB;
};
class NetworkLink {
 public:
  bool begin(const NetworkSettings& settings);
  bool telemetry(const char* json);
  bool incident(const char* json);
  bool command(NetCommand& out);
  bool status(NetStatus& out);
 private:
  static void taskEntry(void* self);
  static void callback(char* topic, uint8_t* payload, unsigned int length);
  void run();
  void message(const char* topic, const uint8_t* payload, size_t length);
  void coapStep();
  bool queueAction(NetAction action, const char* id);
  static NetworkLink* instance_;
  NetworkSettings settings_{};
  QueueHandle_t telemetry_ = nullptr, incidents_ = nullptr, commands_ = nullptr, status_ = nullptr;
  NetStatus state_;
  WiFiClient tcp_;
  PubSubClient mqtt_{tcp_};
  WiFiUDP udp_;
  char lastCommand_[48] = {};
  struct CachedPut {
    bool used = false; IPAddress ip; uint16_t port = 0, mid = 0;
    uint32_t seen = 0, hash = 0; uint8_t reply[64] = {}; size_t size = 0;
  } cache_[16];
  uint16_t reportMid_ = 0;
};
}  // namespace aether
