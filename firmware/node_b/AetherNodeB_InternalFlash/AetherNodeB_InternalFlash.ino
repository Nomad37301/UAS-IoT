#include <Arduino.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <SPI.h>
#include <LittleFS.h>
#include <esp_random.h>
#include <time.h>
#include <Mlx90614.h>
#include <VoltageSensor.h>
#include <ActiveBuzzer.h>
#include <DebouncedButton.h>
#include <RecoveryRelay.h>
#include <StatusRgb.h>
#include <ModbusRtuMaster.h>
#include <AetherRfPacket.h>
#include <NodeBRadio.h>
#include <BlackboxLogger.h>
#include <NetworkLink.h>
#include <Diagnosis.h>
#include <RecoveryPolicy.h>
#include "Config.h"
 
using namespace aether;
Mlx90614 temperature(Wire);
VoltageSensor voltage(4, AETHER_VOLTAGE_GAIN, AETHER_VOLTAGE_OFFSET_V);
ActiveBuzzer buzzer(15);
DebouncedButton muteButton(0);
RecoveryRelay relay(16, RelayPolarity::ActiveHigh);
StatusRgb rgb(48, 24);
ModbusRtuMaster modbus(Serial1);
RF24 chip(14, 10, 4000000);
NodeBRadio radio(chip);
CommandSequence commands;
NetworkLink network;
NetStatus net;
TemperatureReading temp;
VoltageReading volts;
HostMetrics host;
bool sensorReady = false, modbusOk = false;
uint32_t modbusAt = 0, sensorAt = 0, sampledAt = 0, publishedAt = 0, rfAt = 0;
uint16_t rfSeq = 0;
Diagnosis diagnosis{STATE_NETWORK_DEAD, "STARTING"};
RecoveryPolicy recoveryPolicy;
uint32_t rfRetryAt = 0;
char serialLine[24] = {}; size_t serialLength = 0; bool discardSerial = false;
struct LogJob { char event[25]; char message[97]; uint32_t at; };
QueueHandle_t logs;

void loggerTask(void*) {
  BlackboxLogger logger(LittleFS);
  // Flash internal ESP32-S3; tidak memerlukan kartu atau driver microSD.
  if (!LittleFS.begin(false)) {
    Serial.println("[LOG] LittleFS mount failed; initialize with sketch 09 using the same partition layout");
  } else if (LittleFS.totalBytes() < 128 * 1024) {
    Serial.println("[LOG] LittleFS partition too small; need at least 128 KiB");
  } else {
    auto result = logger.begin(esp_random());
    if (result == LogError::None) {
      result = logger.append(millis(), "BOOT", "Integrated Node B started; logging to internal flash");
    }
    if (result == LogError::None) {
      Serial.printf("[LOG] LittleFS ready (%lu KiB): %s, %s\n",
                    static_cast<unsigned long>(LittleFS.totalBytes() / 1024),
                    kBlackboxCurrent, kBlackboxPrevious);
    } else {
      Serial.printf("[LOG] LittleFS initialization failed: %s\n", BlackboxLogger::errorName(result));
    }
  }
  LogJob job;
  for (;;) {
    if (xQueueReceive(logs, &job, portMAX_DELAY) == pdPASS) {
      const auto result = logger.append(job.at, job.event, job.message);
      if (result != LogError::None) Serial.printf("[LOG] %s\n", BlackboxLogger::errorName(result));
    }
  }
}

void logEvent(const char* event, const char* message) {
  LogJob job{}; job.at = millis(); strlcpy(job.event, event, sizeof(job.event));
  strlcpy(job.message, message, sizeof(job.message));
  if (!logs || xQueueSend(logs, &job, 0) != pdPASS) Serial.println("[LOG] queue full/unavailable");
}

void incident(const char* action) {
  char evidence[180];
  snprintf(evidence, sizeof(evidence), "HTTP=%u MODBUS=%s VOLT=%.2f IR=%.1f", net.http,
           modbusOk ? "OK" : "FAIL", volts.voltage_v, temp.ir_obj_c);
  logEvent("INCIDENT", diagnosis.reason);
  JsonDocument doc; const time_t epoch = time(nullptr);
  if (epoch > 1600000000) doc["timestamp"] = epoch; else doc["timestamp"] = nullptr;
  doc["uptime_ms"] = millis(); doc["source"] = "nodeB"; doc["diagnosis"] = diagnosis.reason;
  doc["evidence"] = evidence; doc["action"] = action;
  char output[768]; serializeJson(doc, output, sizeof(output));
  if (!network.incident(output)) Serial.println("[MQTT] incident queue full/unavailable; blackbox queued");
  Serial.println(output);
}

void recover(const char* source) {
  const auto result = relay.trigger(millis());
  logEvent("RECOVERY", RecoveryRelay::resultName(result));
  if (result == RelayTriggerResult::Started) incident(source);
}

void radioStep(uint32_t now) {
  radio.poll(now); RadioTx result;
  if (radio.takeTxResult(result)) {
    if (result != RadioTx::Acked) logEvent("RADIO_TX", NodeBRadio::txName(result));
  }
  if (!radio.ready() && !relay.active() && static_cast<uint32_t>(now - rfRetryAt) >= 10000) {
    rfRetryAt = now; radio.begin(SPI, 32);
  }
  for (uint8_t i = 0; i < 3; ++i) {
    AetherRfPacket p{};
    if (!radio.receive(&p, sizeof(p))) break;
    if (validateCommand(p) != CommandCheck::Valid || !commands.accept(p.seq)) continue;
    if (p.cmd == kCmdRecovery) recover("RADIO_RECOVERY_TRIGGERED");
    else { buzzer.setMuted(true, now); logEvent("MUTE", "Radio command"); }
  }
  if (!radio.busy() && static_cast<uint32_t>(now - rfAt) >= 1000) {
    rfAt = now;
    auto p = makeTelemetry(rfSeq, diagnosis.state);
    p.voltage_v = volts.valid ? volts.voltage_v : NAN;
    p.ir_obj_c = temp.valid ? temp.ir_obj_c : NAN; p.ir_amb_c = temp.valid ? temp.ir_amb_c : NAN;
    p.cpu_temp_c = modbusOk ? host.cpu_temp_c : NAN; p.http_code = net.http;
    p.flags = (net.http == 200 ? kFlagHttpOk : 0) | (modbusOk ? kFlagModbusOk : 0) |
              (net.wifi ? kFlagWifiOk : 0) | (relay.active() ? kFlagRelayActive : 0);
    if (radio.send(&p, sizeof(p), now) == RadioSend::Accepted) ++rfSeq;
  }
}

void publishTelemetry(uint32_t now) {
  JsonDocument doc;
  doc["system"] = "AETHER"; doc["node"] = "nodeB";
  doc["state"] = StatusRgb::stateName(diagnosis.state); doc["state_code"] = diagnosis.state;
  doc["diagnosis"] = diagnosis.reason; doc["voltage_v"] = volts.valid ? volts.voltage_v : NAN;
  doc["ir_obj_c"] = temp.valid ? temp.ir_obj_c : NAN; doc["ir_amb_c"] = temp.valid ? temp.ir_amb_c : NAN;
  doc["sensor_ok"] = temp.valid; doc["voltage_ok"] = volts.valid;
  doc["modbus_ok"] = modbusOk; doc["cpu_temp_c"] = modbusOk ? host.cpu_temp_c : NAN;
  if (modbusOk) { doc["cpu_ram_pct"] = host.cpu_ram_pct; doc["cpu_load_pct"] = host.cpu_load_pct;
    doc["uptime_s_low16"] = host.uptime_s_low16; doc["service_running"] = host.service_running; }
  doc["http_code"] = net.http; doc["http_lat_ms"] = net.latency; doc["spi_rf_seq"] = rfSeq;
  doc["relay_active"] = relay.active(); doc["muted"] = buzzer.muted();
  doc["auto_recovery_attempts"] = recoveryPolicy.attempts();
  doc["ip"] = WiFi.localIP().toString(); doc["uptime_ms"] = now;
  doc["demo_overheat"] = net.demoOverheat && static_cast<uint32_t>(now - net.demoAt) < 30000;
  char text[1536]; serializeJson(doc, text, sizeof(text)); network.telemetry(text);
}

void evaluate(uint32_t now) {
  Evidence evidence;
  evidence.voltageValid = volts.valid; evidence.sensorValid = temp.valid;
  evidence.voltage = volts.voltage_v; evidence.ir = temp.ir_obj_c; evidence.maxIr = AETHER_IR_LIMIT_C;
  if (net.demoOverheat && static_cast<uint32_t>(now - net.demoAt) < 30000) evidence.ir = AETHER_IR_LIMIT_C + 10;
  evidence.modbusOk = modbusOk; evidence.wifi = net.wifi; evidence.mqtt = net.mqtt;
  evidence.httpObserved = net.probeAt != 0; evidence.http = net.http;
  evidence.externalFresh = net.externalAt && static_cast<uint32_t>(now - net.externalAt) < 7000;
  evidence.externalHttp = net.externalHttp;
  evidence.externalRadioAlive = net.externalRadioAlive;
  const auto next = diagnose(evidence);
  if (next.state != diagnosis.state || strcmp(next.reason, diagnosis.reason)) {
    diagnosis = next;
    rgb.setState(diagnosis.state, now);
    buzzer.setPattern(diagnosis.state == STATE_HEALTHY ? BuzzerPattern::Off :
                     diagnosis.state == STATE_HW_ANOMALY || diagnosis.state == STATE_SERVICE_DOWN ?
                     BuzzerPattern::Critical : BuzzerPattern::Warning, now);
    if (diagnosis.state != STATE_HEALTHY) incident("STATE_CHANGED");
    else { logEvent("HEALTHY", "All observed probes healthy"); }
  }
  const auto action = recoveryPolicy.evaluate(now, diagnosis.state, !modbus.busy(),
                                               !relay.active() && relay.cooldownRemainingMs(now) == 0);
  if (AETHER_AUTO_RECOVERY && action == RecoveryAction::SerialRestart &&
      modbus.request(ModbusOperation::RestartService, micros()) == ModbusStart::Accepted) {
    recoveryPolicy.commit(action, now); incident("MODBUS_RESTART_REQUESTED");
  } else if (AETHER_AUTO_RECOVERY && action == RecoveryAction::RelayPulse &&
             relay.trigger(now) == RelayTriggerResult::Started) {
    recoveryPolicy.commit(action, now); incident("AUTO_RELAY_RECOVERY_TRIGGERED");
  }
}

void serialStep() {
  for (uint8_t i = 0; i < 24 && Serial.available(); ++i) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') {
      if (!discardSerial && serialLength) {
        serialLine[serialLength] = 0;
        if (!strcmp(serialLine, "m")) buzzer.setMuted(true, millis());
        else if (!strcmp(serialLine, "u")) buzzer.setMuted(false, millis());
        else if (!strcmp(serialLine, "r")) recover("USB_RECOVERY_TRIGGERED");
        else if (!strcmp(serialLine, "i")) { publishTelemetry(millis()); Serial.println(diagnosis.reason); }
      }
      serialLength = 0; discardSerial = false;
    } else if (!discardSerial) {
      if (!c || serialLength >= sizeof(serialLine) - 1) discardSerial = true;
      else serialLine[serialLength++] = c;
    }
  }
}

void setup() {
  relay.begin(); buzzer.begin(); muteButton.begin(millis()); rgb.begin(); voltage.begin();
  Serial.begin(115200); Serial.println("AETHER integrated Node B; commands m/u/r/i + newline");
  sensorReady = Wire.begin(8, 9, 100000); Wire.setTimeOut(50);
  Serial1.setRxBufferSize(256); Serial1.setTxBufferSize(64); Serial1.begin(9600, SERIAL_8N1, 18, 17);
  if (Serial1) { Serial1.setRxFIFOFull(1); Serial1.setRxTimeout(1); modbus.begin(micros()); }
  SPI.begin(12, 13, 11, 10); if (!radio.begin(SPI, 32)) Serial.println("[RF] init failed");
  logs = xQueueCreate(8, sizeof(LogJob));
  if (logs) xTaskCreatePinnedToCore(loggerTask, "aether-blackbox", 6144, nullptr, 1, nullptr, 0);
  const NetworkSettings settings{AETHER_WIFI_SSID, AETHER_WIFI_PASSWORD, AETHER_PI_HOST,
                                AETHER_MQTT_USER, AETHER_MQTT_PASSWORD, "nodeB", true};
  if (!network.begin(settings)) Serial.println("[NET] task creation failed");
}

void loop() {
  const uint32_t now = millis();
  relay.update(now); buzzer.update(now); rgb.update(now);
  if (muteButton.update(now)) { buzzer.setMuted(true, now); logEvent("MUTE", "BOOT button"); }
  radioStep(now); network.status(net);
  NetCommand cmd;
  for (uint8_t i = 0; i < 8 && network.command(cmd); ++i) {
    if (cmd.action == NetAction::Recovery) recover("NETWORK_RECOVERY_TRIGGERED");
    else { buzzer.setMuted(true, now); logEvent("MUTE", "MQTT/CoAP command"); }
  }
  modbus.poll(micros()); ModbusResult result;
  if (modbus.takeResult(result)) {
    if (result.operation == ModbusOperation::ReadMetrics) {
      modbusOk = result.error == ModbusError::None && result.metrics.valid;
      if (modbusOk) { host = result.metrics; modbusAt = now; }
    } else logEvent("MODBUS_RECOVERY", ModbusRtuMaster::errorName(result.error));
  }
  if (modbusOk && static_cast<uint32_t>(now - modbusAt) > 5000) modbusOk = false;
  if (!modbus.busy() && static_cast<uint32_t>(now - sampledAt) >= 2000) {
    sampledAt = now; modbus.request(ModbusOperation::ReadMetrics, micros());
  }
  if (static_cast<uint32_t>(now - sensorAt) >= 1000 && !relay.active()) {
    sensorAt = now;
    if (sensorReady) temperature.read(temp);
    voltage.read(volts);
    evaluate(now);
  }
  if (static_cast<uint32_t>(now - publishedAt) >= 2000) { publishedAt = now; publishTelemetry(now); }
  serialStep(); delay(1);
}
