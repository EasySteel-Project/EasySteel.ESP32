#include "network.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "tcp_client.h"
#include "offline_buffer.h"
#include "storage.h"
#include "display.h"

#include <WiFi.h>

static bool provisioning = false;
static String deviceName;
static String serverIP;
static String chipId;
static String apPassword;
static uint16_t tcpPort = DEFAULT_TCP_PORT;

// Returns true if device is currently in AP provisioning mode
bool networkIsProvisioning()
{
  return provisioning;
}

void networkInit(Button *abortBtn)
{
  wifiManagerInit(chipId, apPassword);

  storageLoadDevice(deviceName, serverIP, tcpPort, chipId);

  String ssid, pass;
  if (storageLoadWiFi(ssid, pass))
  {
    // Attempt connecting to the saved Wi-Fi network in Station (STA) mode
    if (wifiManagerTryConnect(ssid, pass, deviceName, abortBtn))
    {
      provisioning = false;
      webServerInitSTA(deviceName, serverIP, tcpPort, apPassword);
      tcpClientInit();
      displayUpdate(deviceName.c_str(), WiFi.localIP().toString().c_str());
      return;
    }

    // Saved WiFi unreachable or user skipped -> enter Offline Standby directly (no AP hotspot!)
    Serial.println("[NETWORK] Saved WiFi unreachable/skipped -> Entering Offline Standby");
    provisioning = false;
    WiFi.mode(WIFI_OFF);
    return;
  }

  // AP Provisioning Mode fallback (only when no credentials exist!)
  provisioning = true;
  wifiManagerStartAP(chipId, apPassword);
  webServerInitAP(deviceName, apPassword);
}

bool networkReconnectWiFi(Button *abortBtn)
{
  String ssid, pass;
  if (!storageLoadWiFi(ssid, pass))
  {
    return false;
  }

  if (wifiManagerTryConnect(ssid, pass, deviceName, abortBtn))
  {
    provisioning = false;
    webServerInitSTA(deviceName, serverIP, tcpPort, apPassword);
    tcpClientInit();
    return true;
  }

  return false;
}

void networkStartAPMode()
{
  provisioning = true;
  wifiManagerStartAP(chipId, apPassword);
  webServerInitAP(deviceName, apPassword);
}


void networkTick(unsigned long currentMillis)
{
  bool wpsPending = webServerCheckAndClearWpsPending();
  wifiManagerTick(currentMillis, provisioning, wpsPending, chipId, apPassword);
  webServerTick();
  tcpClientTick(currentMillis, provisioning, serverIP, tcpPort, chipId, deviceName);
}

void networkStartWPS()
{
  wifiManagerStartWPS();
}

void networkSendEvent(const char *event, int target, unsigned long timeMs)
{
  tcpClientSendEvent(chipId, deviceName, provisioning, event, target, timeMs);
}

void networkSendResult(
  unsigned long rawTimeMs,
  unsigned long timeMs,
  int missCount,
  int penaltyCount,
  const char *contextId)
{
  tcpClientSendResult(
    provisioning,
    rawTimeMs,
    timeMs,
    missCount,
    penaltyCount,
    contextId);
}

bool networkIsWiFiConnected()
{
  return wifiManagerIsConnected();
}

bool networkIsServerConnected()
{
  return !provisioning && tcpClientIsConnected();
}

const String &networkGetDeviceName()
{
  return deviceName;
}

const String &networkGetDeviceID()
{
  return chipId;
}

const String &networkGetServerIP()
{
  return serverIP;
}

String networkGetSSID()
{
  return wifiManagerGetSSID();
}

String networkGetLocalIP()
{
  return wifiManagerGetLocalIP();
}

uint16_t networkGetTCPPort()
{
  return tcpPort;
}

void networkResetCredentials()
{
  storageClearWiFi();
}

void networkResetSettings()
{
  storageClearDevice();
}

void networkStopProvisioningAndSetManual()
{
  wifiManagerStopProvisioningAndSetManual(provisioning);
}
