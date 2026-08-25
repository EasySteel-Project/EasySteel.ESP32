#pragma once

#include <Arduino.h>

/// Initialize WebServer in Access Point (AP) mode for WiFi setup.
void webServerInitAP(const String& deviceName, const String& apPassword);

/// Initialize WebServer & OTA in STA mode for settings portal.
void webServerInitSTA(String& deviceName, String& serverIP, uint16_t& tcpPort, const String& apPassword);

/// Tick web server clients and OTA updates.
void webServerTick();

/// Stop and cleanup WebServer instance.
void webServerStop();

/// Check if a deferred WPS start request was triggered via Web UI.
bool webServerCheckAndClearWpsPending();
