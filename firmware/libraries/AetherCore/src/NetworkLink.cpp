#include "NetworkLink.h"
#include "CoapCodec.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <time.h>

namespace aether {
NetworkLink* NetworkLink::instance_ = nullptr;
bool NetworkLink::begin(const NetworkSettings& settings) {
  settings_ = settings;
  telemetry_ = xQueueCreate(1, sizeof(NetPayload)); incidents_ = xQueueCreate(4, sizeof(NetPayload));
  commands_ = xQueueCreate(8, sizeof(NetCommand)); status_ = xQueueCreate(1, sizeof(NetStatus));
  auto cleanup = [this]() {
    for (auto handle : {telemetry_, incidents_, commands_, status_}) if (handle) vQueueDelete(handle);
    telemetry_ = incidents_ = commands_ = status_ = nullptr;
  };
  if (!telemetry_ || !incidents_ || !commands_ || !status_) { cleanup(); return false; }
  instance_ = this;
  if (xTaskCreatePinnedToCore(taskEntry, "aether-network", 14336, this, 1, nullptr, 0) != pdPASS) {
    instance_ = nullptr; cleanup(); return false;
  }
  return true;
}
bool NetworkLink::telemetry(const char* json) {
  if (!telemetry_ || strlen(json) >= sizeof(NetPayload::text)) return false;
  NetPayload p; strlcpy(p.text, json, sizeof(p.text)); return xQueueOverwrite(telemetry_, &p) == pdPASS;
}
bool NetworkLink::incident(const char* json) {
  if (!incidents_ || strlen(json) >= sizeof(NetPayload::text)) return false;
  NetPayload p; strlcpy(p.text, json, sizeof(p.text)); return xQueueSend(incidents_, &p, 0) == pdPASS;
}
bool NetworkLink::command(NetCommand& out) { return commands_ && xQueueReceive(commands_, &out, 0) == pdPASS; }
bool NetworkLink::status(NetStatus& out) { return status_ && xQueuePeek(status_, &out, 0) == pdPASS; }
void NetworkLink::taskEntry(void* self) { static_cast<NetworkLink*>(self)->run(); }
void NetworkLink::callback(char* topic, uint8_t* payload, unsigned int length) { instance_->message(topic, payload, length); }
bool NetworkLink::queueAction(NetAction action, const char* id) {
  if (id && *id && strcmp(lastCommand_, id) == 0) return true;
  NetCommand command{}; command.action = action;
  strlcpy(command.requestId, id ? id : "", sizeof(command.requestId));
  if (xQueueSend(commands_, &command, 0) != pdPASS) return false;
  if (id && *id) strlcpy(lastCommand_, id, sizeof(lastCommand_));
  return true;
}
void NetworkLink::message(const char* topic, const uint8_t* payload, size_t length) {
  if (length > 1535) return;
  JsonDocument doc;
  if (deserializeJson(doc, payload, length)) return;
  if (strcmp(topic, "aether/nodeC/telemetry") == 0 && settings_.nodeB) {
    if (strcmp(doc["system"] | "", "AETHER") || strcmp(doc["node"] | "", "nodeC")) return;
    if (!doc["http_ext_code"].is<unsigned int>()) return;
    const unsigned code = doc["http_ext_code"].as<unsigned>();
    if (code > 599) return;
    state_.externalHttp = code; state_.externalAt = millis();
    state_.externalRadioAlive = doc["rf_rx_alive"] == true;
  } else if (strcmp(topic, "aether/demo/fault") == 0 && settings_.nodeB) {
    const char* mode = doc["mode"] | "";
    if (!*mode) return;
    state_.demoOverheat = strcmp(mode, "OVERHEAT_SIM") == 0; state_.demoAt = millis();
  } else if (strcmp(topic, "aether/cmd/actuator") == 0 && settings_.nodeB) {
    const char* action = doc["action"] | ""; const char* id = doc["request_id"] | "";
    if (strlen(id) >= sizeof(lastCommand_)) return;
    if (!strcmp(action, "TRIGGER_RECOVERY")) queueAction(NetAction::Recovery, id);
    if (!strcmp(action, "MUTE_ALARM") || doc["mute_buzzer"] == true) queueAction(NetAction::Mute, "");
  }
}
void NetworkLink::coapStep() {
  const int length = udp_.parsePacket();
  if (length <= 0) return;
  uint8_t raw[1792];
  const IPAddress peer = udp_.remoteIP(); const uint16_t port = udp_.remotePort();
  if (length > static_cast<int>(sizeof(raw))) { udp_.clear(); return; }
  const int received = udp_.read(raw, sizeof(raw));
  CoapMessage request;
  if (received < 0 || !parseCoap(raw, received, request)) return;
  if (request.type == 2 && request.mid == reportMid_ && request.tokenLength == 2 &&
      request.token[0] == uint8_t(reportMid_ >> 8) && request.token[1] == uint8_t(reportMid_) && request.code == 68) {
    state_.coap = true; return;
  }
  if (request.type > 1 || request.code == 0 || request.code >= 32) return;
  const uint32_t now = millis();
  uint32_t hash = 2166136261U;
  for (int i = 0; i < received; ++i) hash = (hash ^ raw[i]) * 16777619U;
  CachedPut* freeSlot = nullptr;
  for (auto& entry : cache_) {
    if (entry.used && static_cast<uint32_t>(now - entry.seen) >= 247000) entry.used = false;
    if (!entry.used) freeSlot = &entry;
    else if (entry.ip == peer && entry.port == port && entry.mid == request.mid) {
      if (entry.hash == hash) { udp_.beginPacket(peer, port); udp_.write(entry.reply, entry.size); udp_.endPacket(); }
      return;
    }
  }
  uint8_t code = 132; char body[256] = "not found"; bool json = false;
  NetPayload latest; JsonDocument metrics;
  if (xQueuePeek(telemetry_, &latest, 0) == pdPASS) deserializeJson(metrics, latest.text);
  if (request.unsupportedOption) { code = 130; strlcpy(body, "bad option", sizeof(body)); }
  else if (!strcmp(request.path, "/3/0") && request.code == 1) {
    JsonDocument reply; reply["mfg"] = "AETHER"; reply["model"] = "S3-Watchdog";
    reply["state"] = metrics["state_code"]; reply["v"] = metrics["voltage_v"];
    serializeJson(reply, body, sizeof(body)); code = 69; json = true;
  } else if (!strcmp(request.path, "/3303/0/5700") && request.code == 1) {
    if (metrics["ir_obj_c"].isNull()) { code = 163; strlcpy(body, "sensor unavailable", sizeof(body)); }
    else { snprintf(body, sizeof(body), "%.2f", metrics["ir_obj_c"].as<float>()); code = 69; }
  } else if (!strcmp(request.path, "/3311/0/5850") && request.code == 3) {
    if (!freeSlot) { code = 163; strlcpy(body, "duplicate cache full", sizeof(body)); }
    else if (request.payloadLength != 1 || (request.payload[0] != '0' && request.payload[0] != '1')) {
      code = 128; strlcpy(body, "expected 0 or 1", sizeof(body));
    } else {
      const bool queued = queueAction(request.payload[0] == '1' ? NetAction::Recovery : NetAction::Mute, "");
      code = queued ? 68 : 163; strlcpy(body, queued ? "queued" : "queue full", sizeof(body));
    }
  } else if (!strcmp(request.path, "/3/0") || !strcmp(request.path, "/3303/0/5700") || !strcmp(request.path, "/3311/0/5850")) {
    code = 133; strlcpy(body, "method not allowed", sizeof(body));
  }
  uint8_t reply[288]; const size_t size = coapResponse(request, code, body, json, reply, sizeof(reply));
  if (!size) return;
  if (request.code == 3 && freeSlot && size <= sizeof(freeSlot->reply)) {
    freeSlot->used = true; freeSlot->ip = peer; freeSlot->port = port; freeSlot->mid = request.mid;
    freeSlot->seen = now; freeSlot->hash = hash; freeSlot->size = size; memcpy(freeSlot->reply, reply, size);
  }
  udp_.beginPacket(peer, port); udp_.write(reply, size); udp_.endPacket();
}
void NetworkLink::run() {
  WiFi.persistent(false); WiFi.setAutoReconnect(false); WiFi.mode(WIFI_STA); WiFi.setHostname(settings_.node);
  mqtt_.setServer(settings_.host, 1883); mqtt_.setCallback(callback);
  mqtt_.setBufferSize(2048); mqtt_.setSocketTimeout(1); mqtt_.setKeepAlive(15);
  char telemetryTopic[48], statusTopic[48];
  snprintf(telemetryTopic, sizeof(telemetryTopic), "aether/%s/telemetry", settings_.node);
  snprintf(statusTopic, sizeof(statusTopic), "aether/%s/status", settings_.node);
  uint32_t wifiAt = millis() - 15000, mqttAt = millis() - 5000, probeAt = millis() - 2000;
  uint32_t publishAt = 0, reportAt = 0; bool udpStarted = false;
  NetPayload pendingIncident; bool hasIncident = false;
  for (;;) {
    const uint32_t now = millis(); state_.wifi = WiFi.status() == WL_CONNECTED;
    if (!state_.wifi) {
      state_.mqtt = false; state_.coap = false; state_.http = 0;
      if (udpStarted) { udp_.stop(); udpStarted = false; }
      if (*settings_.ssid && static_cast<uint32_t>(now - wifiAt) >= 15000) {
        wifiAt = now; WiFi.disconnect(); WiFi.begin(settings_.ssid, settings_.password);
      }
    } else {
      if (!udpStarted && settings_.nodeB) udpStarted = udp_.begin(5683);
      if (!mqtt_.connected() && static_cast<uint32_t>(now - mqttAt) >= 5000) {
        mqttAt = now;
        if (mqtt_.connect(settings_.node, *settings_.mqttUser ? settings_.mqttUser : nullptr,
                          *settings_.mqttUser ? settings_.mqttPassword : nullptr, statusTopic, 1, true, "offline")) {
          mqtt_.publish(statusTopic, "online", true);
          mqtt_.subscribe("aether/cmd/actuator");
          if (settings_.nodeB) { mqtt_.subscribe("aether/nodeC/telemetry"); mqtt_.subscribe("aether/demo/fault"); }
          configTime(0, 0, "pool.ntp.org");
        }
      }
      mqtt_.loop(); state_.mqtt = mqtt_.connected();
      if (static_cast<uint32_t>(now - probeAt) >= 2000) {
        probeAt = now; WiFiClient probe; HTTPClient http;
        http.setConnectTimeout(500); http.setTimeout(500);
        String url = String("http://") + settings_.host + ":8000/health?probe=" + (settings_.nodeB ? "B" : "C");
        const uint32_t began = millis();
        int code = http.begin(probe, url) ? http.GET() : 0;
        state_.latency = millis() - began; state_.http = code > 0 && code <= 599 ? code : 0;
        state_.probeAt = millis(); http.end();
      }
      if (state_.mqtt && static_cast<uint32_t>(now - publishAt) >= 2000) {
        NetPayload latest;
        if (xQueuePeek(telemetry_, &latest, 0) == pdPASS) mqtt_.publish(telemetryTopic, latest.text);
        publishAt = now;
      }
      if (!hasIncident) hasIncident = xQueueReceive(incidents_, &pendingIncident, 0) == pdPASS;
      if (hasIncident && state_.mqtt && mqtt_.publish("aether/alert/incident", pendingIncident.text)) hasIncident = false;
      if (udpStarted) {
        coapStep();
        if (static_cast<uint32_t>(now - reportAt) >= 10000) {
          reportAt = now; NetPayload latest; uint8_t raw[1600];
          if (xQueuePeek(telemetry_, &latest, 0) == pdPASS) {
            ++reportMid_; const size_t size = coapReport(reportMid_, latest.text, raw, sizeof(raw));
            udp_.beginPacket(settings_.host, 5683); udp_.write(raw, size); udp_.endPacket(); state_.coap = false;
          }
        }
      }
    }
    xQueueOverwrite(status_, &state_);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
}  // namespace aether
