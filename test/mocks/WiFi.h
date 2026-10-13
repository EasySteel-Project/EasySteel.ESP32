#pragma once
#include "Arduino.h"
struct MockWiFi { String macAddress() const { return "00:11:22:33:44:55"; } };
static MockWiFi WiFi;
struct MockEsp { void restart() {} };
static MockEsp ESP;
