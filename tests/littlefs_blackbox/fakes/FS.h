#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>

#define FILE_READ "r"
#define FILE_APPEND "a"

namespace fs {
struct Faults {
  bool open = false, seek = false, read = false, remove = false, rename = false;
  size_t writeLimit = static_cast<size_t>(-1);
  unsigned flushes = 0;
};
struct Entry { std::string bytes; bool directory = false; };

class File {
 public:
  File() = default;
  File(std::shared_ptr<Entry> entry, Faults* faults) : entry_(entry), faults_(faults) {}
  explicit operator bool() const { return static_cast<bool>(entry_); }
  bool isDirectory() { return entry_->directory; }
  size_t size() const { return entry_->bytes.size(); }
  bool seek(uint32_t position) {
    if (faults_->seek || position > size()) return false;
    position_ = position;
    return true;
  }
  int read() {
    if (faults_->read || position_ >= size()) return -1;
    return static_cast<uint8_t>(entry_->bytes[position_++]);
  }
  size_t write(const uint8_t* bytes, size_t count) {
    const size_t actual = std::min(count, faults_->writeLimit);
    entry_->bytes.append(reinterpret_cast<const char*>(bytes), actual);
    return actual;
  }
  void flush() { ++faults_->flushes; }
  void close() { entry_.reset(); }
 private:
  std::shared_ptr<Entry> entry_;
  Faults* faults_ = nullptr;
  size_t position_ = 0;
};

class FS {
 public:
  std::map<std::string, std::shared_ptr<Entry>> files;
  Faults faults;
  bool exists(const char* path) { return files.count(path) != 0; }
  File open(const char* path, const char* mode) {
    if (faults.open) return {};
    if (!exists(path)) {
      if (std::string(mode) == FILE_READ) return {};
      files[path] = std::make_shared<Entry>();
    }
    return File(files.at(path), &faults);
  }
  bool remove(const char* path) { return !faults.remove && files.erase(path) != 0; }
  bool rename(const char* from, const char* to) {
    if (faults.rename || !exists(from) || exists(to)) return false;
    files[to] = files.at(from);
    files.erase(from);
    return true;
  }
  void seed(const char* path, const std::string& content) {
    files[path] = std::make_shared<Entry>();
    files[path]->bytes = content;
  }
  const std::string& bytes(const char* path) const { return files.at(path)->bytes; }
};
}  // namespace fs

