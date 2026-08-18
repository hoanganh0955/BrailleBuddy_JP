#pragma once

#include <cstdint>

class Preferences {
 public:
  bool begin(const char*, bool) { return true; }
  bool getBool(const char*, bool fallback) const { return fallback; }
  uint8_t getUChar(const char*, uint8_t fallback) const { return fallback; }
  bool putBool(const char*, bool) { return true; }
  size_t putUChar(const char*, uint8_t) { return 1U; }
  bool clear() { return true; }
};
