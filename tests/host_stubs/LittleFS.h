#pragma once

#include <cstddef>
#include <cstdint>

constexpr int SeekSet = 0;

class File {
 public:
  explicit operator bool() const { return true; }
  size_t read(uint8_t*, size_t count) { return count; }
  uint32_t size() const { return 0U; }
  uint32_t position() const { return 0U; }
  bool seek(uint32_t, int) { return true; }
  void close() {}
};

class LittleFSStub {
 public:
  bool begin(bool) { return true; }
  File open(const char*, const char*) { return File(); }
};

inline LittleFSStub LittleFS;
