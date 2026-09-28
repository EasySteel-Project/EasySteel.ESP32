#pragma once

#include "config.h"

/// True if ESP32 is in provisioning (AP) mode.
bool networkIsProvisioning();

/// True if WiFi is currently connected in STA mode.
bool networkIsWiFiConnected();

/// True if the TCP timing server is connected.
bool networkIsServerConnected();

/// Stop AP/Webserver provisioning and switch to manual offline mode.
void networkStopProvisioningAndSetManual();

class Button;

/// Initialize network: try saved credentials → STA, otherwise → AP + config portal.
void networkInit(Button *abortBtn = nullptr);

/// Reconnect to saved WiFi network from settings menu. Returns true if connected.
bool networkReconnectWiFi(Button *abortBtn = nullptr);

/// Start AP mode with config portal (without erasing saved credentials).
void networkStartAPMode();

/// Handle network tasks (HTTP in both modes, TCP timing server in STA mode).
void networkTick(unsigned long currentMillis);

/// Start WPS push-button negotiation.
void networkStartWPS();

/// Send a best-effort JSON event to the connected server.
void networkSendEvent(const char *event, int target, unsigned long timeMs);

/// Commit a physically confirmed result. False leaves it pending on the device.
bool networkSendResult(
  unsigned long rawTimeMs,
  unsigned long timeMs,
  int missCount,
  int penaltyCount,
  const char *contextId);

/// Get the device name (e.g. "Bahn 1"). Falls back to MAC-based ID.
const String &networkGetDeviceName();

/// Get the unique device ID based on MAC address.
const String &networkGetDeviceID();

/// Get the configured server IP. Empty if not configured.
const String &networkGetServerIP();

/// Get the current WiFi SSID.
String networkGetSSID();

/// Get the current local IP address.
String networkGetLocalIP();

/// Get the configured TCP Port.
uint16_t networkGetTCPPort();

/// Erase saved WiFi credentials from flash.
void networkResetCredentials();

/// Erase all device settings from flash.
void networkResetSettings();
