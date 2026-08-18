#pragma once

#include <cstddef>
#include <cstdint>

constexpr int I2S_MODE_STD = 0;
constexpr int I2S_DATA_BIT_WIDTH_16BIT = 16;
constexpr int I2S_SLOT_MODE_STEREO = 2;

class I2SClass {
 public:
  void setPins(int8_t, int8_t, int8_t, int8_t, int8_t) {}
  bool begin(int, uint32_t, int, int) { return true; }
  size_t write(uint8_t*, size_t count) { return count; }
};
