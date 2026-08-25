#pragma once

#include <Arduino.h>

/// Initialize WiFi event handlers and chip ID.
void wifiManagerInit(String& chipId, String& apPassword);

/// Start Access Point (AP) mode provisioning.
void wifiManagerStartAP(const String& chipId, const String& apPassword);

class Button;

/// Try connecting to saved WiFi network. Returns true if connected.
bool wifiManagerTryConnect(const String& ssid, const String& pass, const String& hostname = "", Button* abortBtn = nullptr);


/// Start Station (STA) mode.
void wifiManagerStartSTA();

/// Start WPS push-button pairing.
void wifiManagerStartWPS();

/// Tick WiFi/WPS state machine.
void wifiManagerTick(
  unsigned long currentMillis,
  const bool& provisioning,
  bool& wpsPending,
  const String& chipId,
  const String& apPassword);

/// Stop AP provisioning and set device into manual offline mode.
void wifiManagerStopProvisioningAndSetManual(bool& provisioning);

/// Check if WiFi is connected in STA mode.
bool wifiManagerIsConnected();

/// Get SSID.
String wifiManagerGetSSID();

/// Get local IP address string.
String wifiManagerGetLocalIP();
