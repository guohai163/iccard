#pragma once
#include <math.h>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <cstdarg>
#include <cstdio>
using byte = uint8_t;
#define PROGMEM
class __FlashStringHelper;
class String : public std::string {
public:
  using std::string::string;
  String() = default;
  String(const std::string& s) : std::string(s) {}
  String(const __FlashStringHelper* s) : std::string(reinterpret_cast<const char*>(s)) {}
  bool startsWith(const char* prefix) const { return rfind(prefix, 0) == 0; }
  void trim() {
    auto begin = find_first_not_of(" \t\r\n");
    if (begin == npos) { clear(); return; }
    *this = substr(begin, find_last_not_of(" \t\r\n") - begin + 1);
  }
};
inline String operator+(const char* a, const String& b) { return std::string(a) + std::string(b); }
struct FakeSerial {
  std::string output;
  void begin(unsigned) {}
  explicit operator bool() const { return true; }
  void print(const String& s) { output += s; }
  void print(const char* s) { output += s; }
  void print(char c) { output += c; }
  void print(const __FlashStringHelper* s) { output += reinterpret_cast<const char*>(s); }
  template<class T> void println(const T& value) { print(value); output += '\n'; }
  void println() { output += '\n'; }
  void printf(const char* fmt, ...) __attribute__((format(printf,2,3))) {
    char buf[512]; va_list args; va_start(args,fmt); vsnprintf(buf,sizeof(buf),fmt,args); va_end(args); output += buf;
  }
  int available() const { return 0; }
  int read() { return -1; }
};
extern FakeSerial Serial;
extern uint32_t fakeMillis;
inline uint32_t millis() { return fakeMillis; }
inline void delay(unsigned ms) { fakeMillis += ms; }
