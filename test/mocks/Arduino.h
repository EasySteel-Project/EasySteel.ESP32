#ifndef MOCK_ARDUINO_H
#define MOCK_ARDUINO_H

#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <cstdint>

// Pin Modes & States
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define RISING 1
#define FALLING 2
#define CHANGE 3

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

inline int digitalPinToInterrupt(int pin) { return pin; }
extern unsigned int g_mockAttachInterruptCalls;
extern unsigned int g_mockDetachInterruptCalls;
inline void attachInterrupt(int pin, void (*voidFunc)(void), int mode) { ++g_mockAttachInterruptCalls; }
inline void detachInterrupt(int pin) { ++g_mockDetachInterruptCalls; }

// Fake Millis/Micros and Pin state for Mocking
extern unsigned long g_mockMillis;
extern unsigned long g_mockMicros;
extern int g_mockPinState;

inline unsigned long millis() { return g_mockMillis; }
inline unsigned long micros() { return g_mockMicros; }
inline void delay(unsigned long ms) { g_mockMillis += ms; }
inline void delayMicroseconds(unsigned int us) { g_mockMicros += us; }

inline void pinMode(int pin, int mode) {}
inline int digitalRead(int pin) { return g_mockPinState; }
inline void digitalWrite(int pin, int val) {}

// Custom Arduino-like String class for Native compilation
class String : public std::string {
public:
    String() : std::string() {}
    String(const char* s) : std::string(s ? s : "") {}
    String(const std::string& s) : std::string(s) {}
    String(int val) : std::string(std::to_string(val)) {}
    String(long val) : std::string(std::to_string(val)) {}
    String(unsigned long val) : std::string(std::to_string(val)) {}
    String(unsigned int val) : std::string(std::to_string(val)) {}

    size_t length() const { return size(); }
    const char* c_str() const { return std::string::c_str(); }

    int indexOf(char ch, size_t fromIndex = 0) const {
        size_t pos = find(ch, fromIndex);
        return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
    }

    int indexOf(const String& str, size_t fromIndex = 0) const {
        size_t pos = find(str, fromIndex);
        return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
    }

    String substring(size_t from, size_t to = std::string::npos) const {
        if (from >= size()) return String("");
        if (to == std::string::npos || to > size()) return String(substr(from));
        return String(substr(from, to - from));
    }

    bool startsWith(const String& prefix) const {
        return size() >= prefix.size() && compare(0, prefix.size(), prefix) == 0;
    }

    bool endsWith(const String& suffix) const {
        return size() >= suffix.size() && compare(size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    int toInt() const {
        try {
            return std::stoi(*this);
        } catch (...) {
            return 0;
        }
    }
};

// Fake Serial Output
class MockSerial {
public:
    template<typename T>
    void print(const T& val) {
        std::cout << val;
    }
    template<typename T>
    void println(const T& val) {
        std::cout << val << "\n";
    }
    void println() {
        std::cout << "\n";
    }
    void printf(const char* format, ...) {}
};

extern MockSerial Serial;

#endif // MOCK_ARDUINO_H
