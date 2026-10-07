#pragma once

#include <Arduino.h>

/// Initialize WebServer in Access Point (AP) mode for WiFi setup.
void webServerInitAP(const String& deviceName, const String& apPassword);

/// Initialize WebServer & OTA in STA mode for settings portal.
void webServerInitSTA(String& deviceName, String& serverIP, uint16_t& tcpPort, const String& apPassword);

/// Process bounded web work on the loop task and service idle-only OTA.
void webServerTick();

/// Stop and cleanup WebServer instance.
void webServerStop();

/// Check if a deferred WPS start request was triggered via Web UI.
bool webServerCheckAndClearWpsPending();

/// True while firmware maintenance or a scheduled restart excludes round starts.
bool webServerMaintenanceActive();

/// Publish a consistent round/session snapshot from the loop task.
void webServerPublishState();
