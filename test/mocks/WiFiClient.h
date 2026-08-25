#ifndef MOCK_WIFI_CLIENT_H
#define MOCK_WIFI_CLIENT_H

#include "Arduino.h"
#include <string>

class WiFiClient {
public:
    bool isConnected = true;
    std::string writtenData;

    operator bool() const { return isConnected; }
    bool connected() const { return isConnected; }

    size_t write(uint8_t c) {
        writtenData += (char)c;
        return 1;
    }

    size_t write(const uint8_t *buffer, size_t size) {
        writtenData.append((const char*)buffer, size);
        return size;
    }

    size_t print(const String& s) {
        writtenData += s;
        return s.length();
    }

    size_t print(const char* s) {
        writtenData += s;
        return std::string(s).length();
    }

    size_t println(const String& s) {
        writtenData += s;
        writtenData += "\n";
        return s.length() + 1;
    }

    size_t println() {
        writtenData += "\n";
        return 1;
    }

    void clearWrittenData() {
        writtenData.clear();
    }
};

#endif // MOCK_WIFI_CLIENT_H
