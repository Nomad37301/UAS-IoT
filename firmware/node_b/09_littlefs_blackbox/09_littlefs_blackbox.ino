#include <Arduino.h>
#include <LittleFS.h>
#include <esp_random.h>
#include <string.h>

#include "BlackboxLogger.h"

namespace {
constexpr size_t kMinimumPartitionBytes = 128 * 1024;
constexpr uint32_t kHeartbeatPeriodMs = 10000;
aether::BlackboxLogger logger(LittleFS);
bool mounted = false;
bool autoLogging = false;
uint32_t lastHeartbeatMs = 0;
fs::File dumpFile;
char commandBuffer[24] = {};
size_t commandLength = 0;
bool discardCommand = false;

void printHelp() {
  Serial.println("Send a line: w=test event, a=toggle 10s heartbeat, d=current log, b=previous log, x=stop dump, i=status, m=remount, ?=help");
  Serial.println("FORMAT erases ALL files in the LittleFS partition. Mount failure never auto-formats.");
}

void stopDump() { if (dumpFile) dumpFile.close(); }

void record(const char* event, const char* message) {
  const auto error = logger.append(millis(), event, message);
  Serial.printf("{\"event\":\"LOG_RESULT\",\"result\":\"%s\",\"next_seq\":%lu}\n",
                aether::BlackboxLogger::errorName(error),
                static_cast<unsigned long>(logger.nextSequence()));
}

size_t fileBytes(const char* path) {
  if (!mounted || !LittleFS.exists(path)) return 0;
  auto file = LittleFS.open(path, FILE_READ);
  if (!file) return 0;
  const size_t bytes = file.size();
  file.close();
  return bytes;
}

void printStatus() {
  Serial.printf("{\"system\":\"AETHER\",\"node\":\"nodeB\",\"component\":\"LITTLEFS_BLACKBOX\","
                "\"mounted\":%s,\"logger_ready\":%s,\"session\":\"%08lx\",\"next_seq\":%lu,"
                "\"auto_logging\":%s,\"total_bytes\":%lu,\"used_bytes\":%lu,"
                "\"current_bytes\":%lu,\"previous_bytes\":%lu}\n",
                mounted ? "true" : "false", logger.ready() ? "true" : "false",
                static_cast<unsigned long>(logger.sessionId()),
                static_cast<unsigned long>(logger.nextSequence()), autoLogging ? "true" : "false",
                static_cast<unsigned long>(mounted ? LittleFS.totalBytes() : 0),
                static_cast<unsigned long>(mounted ? LittleFS.usedBytes() : 0),
                static_cast<unsigned long>(fileBytes(aether::kBlackboxCurrent)),
                static_cast<unsigned long>(fileBytes(aether::kBlackboxPrevious)));
}

void mountLogger() {
  stopDump();
  logger.end();
  if (mounted) LittleFS.end();
  mounted = LittleFS.begin(false, "/littlefs", 4, "spiffs");
  if (!mounted) {
    Serial.println("{\"error\":\"MOUNT_FAILED\",\"auto_format\":false}");
    return;
  }
  if (LittleFS.totalBytes() < kMinimumPartitionBytes) {
    Serial.println("{\"error\":\"PARTITION_TOO_SMALL_OR_INFO_FAILED\"}");
    return;
  }
  const auto error = logger.begin(esp_random());
  Serial.printf("{\"event\":\"LOGGER_INIT\",\"result\":\"%s\"}\n",
                aether::BlackboxLogger::errorName(error));
  if (error == aether::LogError::None) record("SESSION_START", "LittleFS blackbox logger started");
}

void startDump(const char* path) {
  if (!mounted) { Serial.println("{\"error\":\"NOT_MOUNTED\"}"); return; }
  if (!LittleFS.exists(path)) { Serial.println("{\"error\":\"LOG_NOT_FOUND\"}"); return; }
  dumpFile = LittleFS.open(path, FILE_READ);
  if (!dumpFile || dumpFile.isDirectory()) {
    stopDump();
    Serial.println("{\"error\":\"DUMP_OPEN_FAILED\"}");
    return;
  }
  Serial.printf("{\"event\":\"DUMP_BEGIN\",\"file\":\"%s\"}\n", path);
}

void serviceDump() {
  if (!dumpFile) return;
  if (dumpFile.available() <= 0) {
    stopDump();
    Serial.println("\n{\"event\":\"DUMP_END\"}");
    return;
  }
  const int capacity = Serial.availableForWrite();
  if (capacity <= 0) return;
  uint8_t bytes[64];
  const size_t count = static_cast<size_t>(capacity) < sizeof(bytes) ? capacity : sizeof(bytes);
  const size_t received = dumpFile.read(bytes, count);
  if (received == 0) {
    stopDump();
    Serial.println("\n{\"error\":\"DUMP_READ_FAILED\"}");
    return;
  }
  Serial.write(bytes, received);
}

void handleCommand(const char* command) {
  if (dumpFile) {
    if (strcmp(command, "x") == 0) {
      stopDump();
      Serial.println("\n{\"event\":\"DUMP_CANCELLED\"}");
    }
    // Saat dump, abaikan perintah lain agar stream tidak bercampur dan file tetap.
    return;
  }
  if (strcmp(command, "w") == 0) record("TEST_EVENT", "Manual blackbox test");
  else if (strcmp(command, "a") == 0) {
    autoLogging = !autoLogging;
    lastHeartbeatMs = millis();
    printStatus();
  } else if (strcmp(command, "d") == 0) startDump(aether::kBlackboxCurrent);
  else if (strcmp(command, "b") == 0) startDump(aether::kBlackboxPrevious);
  else if (strcmp(command, "i") == 0) printStatus();
  else if (strcmp(command, "m") == 0) mountLogger();
  else if (strcmp(command, "?") == 0) printHelp();
  else if (strcmp(command, "FORMAT") == 0) {
    autoLogging = false;
    logger.end();
    if (mounted) LittleFS.end();
    mounted = false;
    const bool ok = LittleFS.format();
    Serial.printf("{\"event\":\"FORMAT\",\"result\":\"%s\"}\n", ok ? "OK" : "FAILED");
    if (ok) mountLogger();
  } else if (strcmp(command, "x") != 0) Serial.println("{\"error\":\"UNKNOWN_COMMAND\"}");
}

void serviceSerial() {
  for (uint8_t i = 0; i < 32 && Serial.available() > 0; ++i) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r' || c == '\n') {
      if (discardCommand) Serial.println("{\"error\":\"COMMAND_TOO_LONG\"}");
      else if (commandLength > 0) {
        commandBuffer[commandLength] = '\0';
        handleCommand(commandBuffer);
      }
      commandLength = 0;
      discardCommand = false;
    } else if (!discardCommand) {
      if (c == '\0' || commandLength >= sizeof(commandBuffer) - 1) discardCommand = true;
      else commandBuffer[commandLength++] = c;
    }
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println("AETHER Node B | Bagian 09: LittleFS Blackbox | 2 files x 32KiB");
  printHelp();
  mountLogger();
  printStatus();
  lastHeartbeatMs = millis();
}

void loop() {
  serviceSerial();
  serviceDump();
  const uint32_t nowMs = millis();
  if (autoLogging && !dumpFile && static_cast<uint32_t>(nowMs - lastHeartbeatMs) >= kHeartbeatPeriodMs) {
    lastHeartbeatMs = nowMs;
    record("HEARTBEAT", "Test heartbeat; sensors and diagnosis not integrated");
  }
  delay(1);
}
