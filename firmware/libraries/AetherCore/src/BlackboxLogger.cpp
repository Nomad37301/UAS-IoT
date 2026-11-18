#include "BlackboxLogger.h"

#include <stdio.h>

namespace aether {

LogError BlackboxLogger::inspect(const char* path, bool current) {
  if (!fs_.exists(path)) return LogError::None;
  auto file = fs_.open(path, FILE_READ);
  if (!file || file.isDirectory()) return LogError::OpenFailed;
  const size_t bytes = file.size();
  if (bytes > kBlackboxFileLimit) { file.close(); return LogError::OversizedFile; }
  if (current) {
    currentBytes_ = bytes;
    if (bytes > 0) {
      if (!file.seek(bytes - 1)) { file.close(); return LogError::ReadFailed; }
      const int last = file.read();
      if (last < 0) { file.close(); return LogError::ReadFailed; }
      incompleteTail_ = last != '\n';
    }
  }
  file.close();
  return LogError::None;
}

LogError BlackboxLogger::begin(uint32_t sessionId) {
  ready_ = false;
  currentBytes_ = 0;
  incompleteTail_ = false;
  sessionId_ = sessionId;
  sequence_ = 0;
  LogError error = inspect(kBlackboxPrevious, false);
  if (error != LogError::None) return error;
  error = inspect(kBlackboxCurrent, true);
  if (error != LogError::None) return error;
  ready_ = true;
  return LogError::None;
}

LogError BlackboxLogger::buildLine(uint32_t uptimeMs, const char* event,
                                  const char* message, char* line, size_t& length) const {
  if (!event || !*event) return LogError::InvalidEvent;
  size_t eventLength = 0;
  while (event[eventLength]) {
    if (eventLength >= kBlackboxEventLimit) return LogError::InvalidEvent;
    const char c = event[eventLength++];
    if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
      return LogError::InvalidEvent;
    }
  }
  if (!message) return LogError::MessageTooLong;
  size_t messageLength = 0;
  while (message[messageLength]) {
    if (messageLength >= kBlackboxMessageLimit) return LogError::MessageTooLong;
    ++messageLength;
  }
  const int prefix = snprintf(line, kBlackboxLineCapacity,
      "{\"system\":\"AETHER\",\"node\":\"nodeB\",\"session\":\"%08lx\",\"seq\":%lu,"
      "\"uptime_ms\":%lu,\"event\":\"%s\",\"message\":\"",
      static_cast<unsigned long>(sessionId_), static_cast<unsigned long>(sequence_),
      static_cast<unsigned long>(uptimeMs), event);
  if (prefix < 0 || static_cast<size_t>(prefix) >= kBlackboxLineCapacity) return LogError::LineTooLong;
  length = static_cast<size_t>(prefix);
  const char* hex = "0123456789abcdef";
  for (size_t i = 0; i < messageLength; ++i) {
    const uint8_t c = static_cast<uint8_t>(message[i]);
    if (length + 7 >= kBlackboxLineCapacity) return LogError::LineTooLong;
    if (c == '"' || c == '\\') {
      line[length++] = '\\'; line[length++] = static_cast<char>(c);
    } else if (c < 0x20) {
      line[length++] = '\\'; line[length++] = 'u';
      line[length++] = '0'; line[length++] = '0';
      line[length++] = hex[c >> 4U]; line[length++] = hex[c & 0x0FU];
    } else {
      line[length++] = static_cast<char>(c);
    }
  }
  if (length + 4 > kBlackboxLineCapacity) return LogError::LineTooLong;
  line[length++] = '"'; line[length++] = '}'; line[length++] = '\n';
  line[length] = '\0';
  return LogError::None;
}

LogError BlackboxLogger::rotate() {
  if (fs_.exists(kBlackboxPrevious) && !fs_.remove(kBlackboxPrevious)) return ioFailure(LogError::RemoveFailed);
  if (fs_.exists(kBlackboxCurrent) && !fs_.rename(kBlackboxCurrent, kBlackboxPrevious)) {
    return ioFailure(LogError::RenameFailed);
  }
  currentBytes_ = 0;
  incompleteTail_ = false;
  return LogError::None;
}

LogError BlackboxLogger::append(uint32_t uptimeMs, const char* event, const char* message) {
  if (!ready_) return LogError::NotReady;
  char line[kBlackboxLineCapacity];
  size_t length = 0;
  LogError error = buildLine(uptimeMs, event, message, line, length);
  if (error != LogError::None) return error;
  if (incompleteTail_ || currentBytes_ + length > kBlackboxFileLimit) {
    error = rotate();
    if (error != LogError::None) return error;
  }
  auto file = fs_.open(kBlackboxCurrent, FILE_APPEND);
  if (!file) return ioFailure(LogError::OpenFailed);
  const size_t written = file.write(reinterpret_cast<const uint8_t*>(line), length);
  file.flush();
  file.close();
  if (written != length) return ioFailure(LogError::WriteFailed);
  currentBytes_ += length;
  ++sequence_;
  return LogError::None;
}

const char* BlackboxLogger::errorName(LogError error) {
  switch (error) {
    case LogError::None: return "OK";
    case LogError::NotReady: return "LOGGER_NOT_READY";
    case LogError::InvalidEvent: return "INVALID_EVENT";
    case LogError::MessageTooLong: return "MESSAGE_TOO_LONG_OR_NULL";
    case LogError::LineTooLong: return "LINE_TOO_LONG";
    case LogError::OpenFailed: return "OPEN_FAILED";
    case LogError::ReadFailed: return "READ_FAILED";
    case LogError::OversizedFile: return "OVERSIZED_FILE";
    case LogError::RemoveFailed: return "ROTATION_REMOVE_FAILED";
    case LogError::RenameFailed: return "ROTATION_RENAME_FAILED";
    case LogError::WriteFailed: return "WRITE_FAILED";
  }
  return "UNKNOWN";
}

}  // namespace aether
