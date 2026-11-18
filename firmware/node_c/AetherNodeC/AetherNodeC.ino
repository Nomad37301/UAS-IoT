#include <Arduino.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <AetherRfPacket.h>
#include <NodeBRadio.h>
#include <DebouncedButton.h>
#include <NetworkLink.h>
#include "Config.h"

using namespace aether;
RF24 chip(4, 5, 4000000);
NodeBRadio radio(chip);
DebouncedButton recoveryButton(33);
Adafruit_SSD1306 display(128, 64, &Wire, -1);
NetworkLink network;
NetStatus net;
AetherRfPacket latest{};
bool havePacket = false, oledOk = false;
uint32_t receivedAt = 0, displayAt = 0, telemetryAt = 0;
uint32_t rfRetryAt = 0;
uint16_t commandSeq = 0;
const char* commandResult = "NONE";
char line[16] = {}; size_t lineSize = 0; bool discardLine = false;

bool rfAlive(uint32_t now) { return havePacket && static_cast<uint32_t>(now - receivedAt) < 5000; }
AetherState visibleState(uint32_t now) {
  if (!rfAlive(now)) return STATE_NETWORK_DEAD;
  if (latest.state == STATE_HEALTHY && net.probeAt && net.http != 200) return STATE_EXT_ISOLATED;
  return static_cast<AetherState>(latest.state);
}
void remoteCommand(uint8_t cmd) {
  auto packet = makeTelemetry(commandSeq, STATE_NETWORK_DEAD);
  packet.msg_type = kRfCommand; packet.cmd = cmd;
  const auto result = radio.send(&packet, sizeof(packet), millis());
  commandResult = NodeBRadio::sendName(result);
  if (result == RadioSend::Accepted) ++commandSeq;
  Serial.printf("{\"event\":\"REMOTE_COMMAND\",\"cmd\":%u,\"result\":\"%s\"}\n", cmd, commandResult);
}
void radioStep(uint32_t now) {
  radio.poll(now); RadioTx result;
  if (radio.takeTxResult(result)) {
    commandResult = NodeBRadio::txName(result);
    Serial.printf("{\"event\":\"COMMAND_TX_RESULT\",\"result\":\"%s\"}\n", commandResult);
  }
  if (!radio.ready() && static_cast<uint32_t>(now - rfRetryAt) >= 10000) {
    rfRetryAt = now; radio.begin(SPI, 32, false);
  }
  for (uint8_t i = 0; i < 3; ++i) {
    AetherRfPacket p{};
    if (!radio.receive(&p, sizeof(p))) break;
    if (p.magic != kRfMagic || p.msg_type != kRfTelemetry || p.state > STATE_HW_ANOMALY ||
        (p.flags & 0xF0) || p.cmd != 0) continue;
    latest = p; havePacket = true; receivedAt = now;
  }
}
void leds(uint32_t now) {
  const auto state = visibleState(now);
  bool green = state == STATE_HEALTHY;
  bool yellow = state == STATE_EXT_ISOLATED;
  bool red = state == STATE_SERVICE_DOWN || state == STATE_HW_ANOMALY;
  if (state == STATE_NETWORK_DEAD) { yellow = (now % 1000) < 500; red = !yellow; }
  digitalWrite(25, green); digitalWrite(26, yellow); digitalWrite(27, red);
}
void number(char* output, size_t capacity, float value) {
  if (isfinite(value)) snprintf(output, capacity, "%.1f", value);
  else strlcpy(output, "--", capacity);
}
void oled(uint32_t now) {
  if (!oledOk || static_cast<uint32_t>(now - displayAt) < 500) return;
  displayAt = now; display.clearDisplay(); display.setCursor(0, 0);
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.println("AETHER ADMIN / NODE C");
  display.printf("STATE %u  RF %s\n", visibleState(now), rfAlive(now) ? "OK" : "LOST");
  char v[12], ir[12], cpu[12];
  number(v, sizeof(v), rfAlive(now) ? latest.voltage_v : NAN);
  number(ir, sizeof(ir), rfAlive(now) ? latest.ir_obj_c : NAN);
  number(cpu, sizeof(cpu), rfAlive(now) ? latest.cpu_temp_c : NAN);
  display.printf("V:%s IR:%s\nCPU:%s C\n", v, ir, cpu);
  display.printf("EXT:%u MQTT:%s\n", net.http, net.mqtt ? "OK" : "--");
  display.printf("CMD:%s\n", commandResult);
  display.printf("SEQ:%u BTN33=RECOVERY", havePacket ? latest.seq : 0);
  display.display();
}
void telemetry(uint32_t now, bool serial = false) {
  JsonDocument doc; doc["system"] = "AETHER"; doc["node"] = "nodeC";
  doc["http_ext_code"] = net.http; doc["http_ext_lat_ms"] = net.latency;
  doc["rf_rx_alive"] = rfAlive(now); doc["last_rf_seq"] = havePacket ? latest.seq : 0;
  if (rfAlive(now)) doc["last_rf_state"] = latest.state; else doc["last_rf_state"] = nullptr;
  doc["local_state"] = visibleState(now); doc["command_result"] = commandResult;
  doc["ip"] = WiFi.localIP().toString();
  char text[768]; serializeJson(doc, text, sizeof(text)); network.telemetry(text);
  if (serial) Serial.println(text);
}
void serialStep() {
  for (uint8_t i = 0; i < 16 && Serial.available(); ++i) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') {
      if (!discardLine && lineSize) {
        line[lineSize] = 0;
        if (!strcmp(line, "r")) remoteCommand(kCmdRecovery);
        else if (!strcmp(line, "m")) remoteCommand(kCmdMute);
        else if (!strcmp(line, "i")) telemetry(millis(), true);
      }
      lineSize = 0; discardLine = false;
    } else if (!discardLine) {
      if (!c || lineSize >= sizeof(line) - 1) discardLine = true;
      else line[lineSize++] = c;
    }
  }
}
void setup() {
  Serial.begin(115200); Serial.println("AETHER Node C; r=recovery m=mute i=status + newline");
  pinMode(25, OUTPUT); pinMode(26, OUTPUT); pinMode(27, OUTPUT);
  digitalWrite(25, LOW); digitalWrite(26, LOW); digitalWrite(27, LOW);
  recoveryButton.begin(millis());
  Wire.begin(21, 22, 100000);
  oledOk = display.begin(SSD1306_SWITCHCAPVCC, 0x3C, true, false);
  SPI.begin(18, 19, 23, 5);
  if (!radio.begin(SPI, 32, false)) Serial.println("[RF] init failed");
  const NetworkSettings settings{AETHER_WIFI_SSID, AETHER_WIFI_PASSWORD, AETHER_PI_HOST,
                                AETHER_MQTT_USER, AETHER_MQTT_PASSWORD, "nodeC", false};
  if (!network.begin(settings)) Serial.println("[NET] task creation failed");
}
void loop() {
  const uint32_t now = millis();
  radioStep(now); network.status(net);
  if (recoveryButton.update(now)) remoteCommand(kCmdRecovery);
  leds(now); oled(now); serialStep();
  if (static_cast<uint32_t>(now - telemetryAt) >= 2000) { telemetryAt = now; telemetry(now); }
  delay(1);
}
