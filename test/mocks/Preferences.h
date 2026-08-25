#ifndef MOCK_PREFERENCES_H
#define MOCK_PREFERENCES_H

#include "Arduino.h"
#include <map>
#include <string>

class Preferences {
private:
    std::string currentNs;
    static std::map<std::string, std::map<std::string, std::string>>& getStore() {
        static std::map<std::string, std::map<std::string, std::string>> store;
        return store;
    }

public:
    bool begin(const char* name, bool readOnly = false) {
        currentNs = name ? name : "";
        return true;
    }

    void end() {
        currentNs.clear();
    }

    String getString(const char* key, const String& defaultValue = "") {
        auto& store = getStore();
        if (store.find(currentNs) != store.end() && store[currentNs].find(key) != store[currentNs].end()) {
            return String(store[currentNs][key]);
        }
        return defaultValue;
    }

    size_t putString(const char* key, const String& value) {
        getStore()[currentNs][key] = value.c_str();
        return value.length();
    }

    uint16_t getUShort(const char* key, uint16_t defaultValue = 0) {
        auto& store = getStore();
        if (store.find(currentNs) != store.end() && store[currentNs].find(key) != store[currentNs].end()) {
            return static_cast<uint16_t>(std::stoi(store[currentNs][key]));
        }
        return defaultValue;
    }

    size_t putUShort(const char* key, uint16_t value) {
        getStore()[currentNs][key] = std::to_string(value);
        return sizeof(value);
    }

    int getInt(const char* key, int defaultValue = 0) {
        auto& store = getStore();
        if (store.find(currentNs) != store.end() && store[currentNs].find(key) != store[currentNs].end()) {
            return std::stoi(store[currentNs][key]);
        }
        return defaultValue;
    }

    size_t putInt(const char* key, int value) {
        getStore()[currentNs][key] = std::to_string(value);
        return sizeof(value);
    }

    bool getBool(const char* key, bool defaultValue = false) {
        auto& store = getStore();
        if (store.find(currentNs) != store.end() && store[currentNs].find(key) != store[currentNs].end()) {
            return store[currentNs][key] == "1";
        }
        return defaultValue;
    }

    size_t putBool(const char* key, bool value) {
        getStore()[currentNs][key] = value ? "1" : "0";
        return sizeof(value);
    }

    void clear() {
        getStore()[currentNs].clear();
    }

    static void resetAllStores() {
        getStore().clear();
    }
};

#endif // MOCK_PREFERENCES_H
