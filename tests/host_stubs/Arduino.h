#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>

#define F(value) value
#define HIGH 1
#define LOW 0
#define INPUT_PULLUP 2

inline uint32_t millis() { return 0; }
inline void delay(uint32_t) {}
inline void yield() {}
inline void pinMode(uint8_t, uint8_t) {}
inline int digitalRead(uint8_t) { return HIGH; }
inline void randomSeed(uint32_t) {}
inline long random(long maximum) { return maximum > 0 ? 0 : 0; }

class String {
 public:
  String() = default;
  String(const char* value) : value_(value == nullptr ? "" : value) {}

  void reserve(size_t capacity) { value_.reserve(capacity); }
  size_t length() const { return value_.length(); }
  const char* c_str() const { return value_.c_str(); }

  void trim() {
    const auto first = value_.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
      value_.clear();
      return;
    }
    const auto last = value_.find_last_not_of(" \t\r\n");
    value_ = value_.substr(first, last - first + 1U);
  }

  void toLowerCase() {
    std::transform(value_.begin(), value_.end(), value_.begin(),
                   [](unsigned char character) {
                     return static_cast<char>(std::tolower(character));
                   });
  }

  bool startsWith(const char* prefix) const {
    return value_.rfind(prefix, 0) == 0;
  }

  String substring(size_t start) const {
    return String(value_.substr(start).c_str());
  }

  String& operator=(const char* value) {
    value_ = value == nullptr ? "" : value;
    return *this;
  }

  String& operator+=(char value) {
    value_ += value;
    return *this;
  }

  friend bool operator==(const String& left, const char* right) {
    return left.value_ == right;
  }

 private:
  std::string value_;
};

class SerialStub {
 public:
  void begin(uint32_t) {}
  int available() const { return 0; }
  int read() const { return -1; }

  template <typename T>
  void print(const T&) const {}

  template <typename T>
  void println(const T&) const {}

  void println() const {}
};

inline SerialStub Serial;
