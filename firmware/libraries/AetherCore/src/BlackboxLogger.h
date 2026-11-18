#pragma once

#include <FS.h>
#include <stdint.h>

namespace aether {

constexpr size_t kBlackboxFileLimit = 32768;
constexpr size_t kBlackboxMessageLimit = 96;
constexpr size_t kBlackboxEventLimit = 24;
constexpr size_t kBlackboxLineCapacity = 768;
constexpr const char* kBlackboxCurrent = "/aether-current.jsonl";
constexpr const char* kBlackboxPrevious = "/aether-previous.jsonl";

enum class LogError : uint8_t {
  None, NotReady, InvalidEvent, MessageTooLong, LineTooLong,
  OpenFailed, ReadFailed, OversizedFile, RemoveFailed, RenameFailed, WriteFailed
};

// FS harus sudah di-mount pemilik. Gunakan dari satu loop, tanpa writer lain
// pada dua file milik logger. Driver tidak pernah memformat filesystem.
class BlackboxLogger {
 public:
  explicit BlackboxLogger(fs::FS& filesystem) : fs_(filesystem) {}
  LogError begin(uint32_t sessionId);
  void end() { ready_ = false; }
  LogError append(uint32_t uptimeMs, const char* event, const char* message);
  bool ready() const { return ready_; }
  uint32_t nextSequence() const { return sequence_; }
  uint32_t sessionId() const { return sessionId_; }
  static const char* errorName(LogError error);

 private:
  LogError inspect(const char* path, bool current);
  LogError rotate();
  LogError ioFailure(LogError error) { ready_ = false; return error; }
  LogError buildLine(uint32_t uptimeMs, const char* event, const char* message,
                     char* line, size_t& length) const;

  fs::FS& fs_;
  bool ready_ = false;
  bool incompleteTail_ = false;
  size_t currentBytes_ = 0;
  uint32_t sessionId_ = 0;
  uint32_t sequence_ = 0;
};

}  // namespace aether

