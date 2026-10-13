#include "wifi_manager.h"
#include "web_server.h"
#include "display.h"
#include "ui_strings.h"
#include "storage.h"
#include "config.h"
#include "button.h"
#include "watchdog.h"

#include <WiFi.h>
#include <esp_system.h>
#include <esp_wps.h>
#include <DNSServer.h>

static bool wpsInProgress = false;
static esp_wps_config_t wpsConfig;
static DNSServer dnsServer;

enum WpsResult { WPS_IDLE, WPS_GOT_CREDENTIALS, WPS_FAILED, WPS_TIMEOUT, WPS_CONNECTED };
static volatile WpsResult wpsResult = WPS_IDLE;

static void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info)
{
  switch (event)
  {
  case ARDUINO_EVENT_WPS_ER_SUCCESS:
    Serial.println("WPS success");
    wpsResult = WPS_GOT_CREDENTIALS;
    break;
  case ARDUINO_EVENT_WPS_ER_FAILED:
    Serial.println("WPS failed");
    wpsResult = WPS_FAILED;
    break;
  case ARDUINO_EVENT_WPS_ER_TIMEOUT:
    Serial.println("WPS timeout");
    wpsResult = WPS_TIMEOUT;
    break;
  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    if (wpsInProgress)
    {
      Serial.println("WPS connected");
      wpsResult = WPS_CONNECTED;
    }
    break;
  default:
    break;
  }
}

bool wifiManagerInit(String& chipId, String& apPassword)
{
  WiFi.onEvent(onWiFiEvent);
  // Enable the radio entropy source before generating a persistent device secret.
  WiFi.mode(WIFI_STA);
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  chipId = "EasySteel_" + mac.substring(8);
  return storageLoadDevicePassword(apPassword, esp_random);
}

void wifiManagerStartAP(const String& chipId, const String& apPassword)
{
  wpsInProgress = false;

  String apName = String(AP_SSID_PREFIX) + chipId.substring(chipId.length() - 4);

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apName.c_str(), apPassword.c_str());

  // Start Captive Portal DNS Server (redirect all domains to ESP32 IP)
  dnsServer.start(53, "*", WiFi.softAPIP());

  Serial.print("AP started: ");
  Serial.println(apName);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  char line2[32];
  snprintf(line2, sizeof(line2), "%s", apPassword.c_str());
  displayUpdate(WiFi.softAPIP().toString().c_str(), line2);
}

bool wifiManagerTryConnect(const String &ssid, const String &pass, const String &hostname, Button *abortBtn)
{
  Serial.print("[WIFI] Connecting to: ");
  Serial.println(ssid);
  displayUpdate(STR_CONNECTING, abortBtn ? STR_WIFI_STOP_OFFLINE : ssid.c_str());

  WiFi.mode(WIFI_STA);
  if (hostname.length() > 0)
  {
    WiFi.setHostname(hostname.c_str());
  }

  WiFi.begin(ssid.c_str(), pass.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    unsigned long now = millis();

    if (abortBtn)
    {
      abortBtn->update(now);
      if (abortBtn->wasPressed() || abortBtn->isDown())
      {
        Serial.println("[WIFI] Connection cancelled by user -> Offline mode");
        WiFi.disconnect();
        return false;
      }
    }

    if (now - start > WIFI_CONNECT_TIMEOUT_MS)
    {
      Serial.println("[WIFI] Connection timeout");
      WiFi.disconnect();
      return false;
    }
    watchdogFeed();
    delay(50);
  }

  dnsServer.stop();

  Serial.print("[WIFI] Connected! IP: ");
  Serial.println(WiFi.localIP());
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  return true;
}


void wifiManagerStartWPS()
{
  if (wpsInProgress)
    return;

  wpsInProgress = true;

  Serial.println("Starting WPS...");
  displayUpdate(STR_WPS_ACTIVE, STR_PRESS_ROUTER_WPS);

  webServerStop();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);

  wpsConfig.wps_type = WPS_TYPE_PBC;
  strcpy(wpsConfig.factory_info.manufacturer, "ESPRESSIF");
  strcpy(wpsConfig.factory_info.model_number, "ESP32");
  strcpy(wpsConfig.factory_info.model_name, "ESPRESSIF IOT");
  strcpy(wpsConfig.factory_info.device_name, "ESP STATION");

  esp_wifi_wps_enable(&wpsConfig);
  esp_wifi_wps_start(0);
}

void wifiManagerTick(
  unsigned long currentMillis,
  const bool& provisioning,
  bool& wpsPending,
  const String& chipId,
  const String& apPassword)
{
  if (wpsInProgress)
  {
    WpsResult result = wpsResult;

    if (result == WPS_GOT_CREDENTIALS)
    {
      wpsResult = WPS_IDLE;
      esp_wifi_wps_disable();
      displayUpdate(STR_WPS_OK, STR_CONNECTING);
      WiFi.begin();
      return;
    }

    if (result == WPS_CONNECTED)
    {
      wpsResult = WPS_IDLE;
      wpsInProgress = false;
      String ssid = WiFi.SSID();
      String pass = WiFi.psk();
      Serial.print("WPS connected to: ");
      Serial.println(ssid);
      storageSaveWiFi(ssid, pass);
      displayUpdate(STR_SAVED, STR_RESTARTING);
      Serial.println("Credentials saved. Restarting...");
      delay(2000);
      ESP.restart();
    }

    if (result == WPS_FAILED || result == WPS_TIMEOUT)
    {
      wpsResult = WPS_IDLE;
      wpsInProgress = false;
      esp_wifi_wps_disable();
      displayUpdate(result == WPS_FAILED ? STR_WPS_FAILED : STR_WPS_TIMEOUT, STR_RESTARTING);
      delay(2000);
      ESP.restart();
    }

    return;
  }

  static unsigned long wpsRequestTime = 0;
  if (provisioning && (wpsPending || wpsRequestTime != 0))
  {
    if (wpsRequestTime == 0)
      wpsRequestTime = currentMillis;
    else if (currentMillis - wpsRequestTime >= 500)
    {
      wpsRequestTime = 0;
      wpsPending = false;
      wifiManagerStartWPS();
    }
  }

  // Handle Captive Portal DNS requests
  if (provisioning)
  {
    dnsServer.processNextRequest();
  }

  // Cycle OLED display information when in Access Point (AP) provisioning mode
  if (provisioning && !wpsInProgress)
  {
    static unsigned long lastToggleMs = 0;
    static bool showIp = false;
    if (lastToggleMs == 0 || currentMillis - lastToggleMs >= 2500)
    {
      lastToggleMs = currentMillis;
      showIp = !showIp;

      const size_t suffixOffset = chipId.length() > 4 ? chipId.length() - 4 : 0;
      char apName[32];
      snprintf(apName, sizeof(apName), "%s%s", AP_SSID_PREFIX, chipId.c_str() + suffixOffset);
      char line1[32];
      snprintf(line1, sizeof(line1), STR_WIFI_AP_NAME_FORMAT, apName);

      char line2[32];
      if (showIp)
      {
        snprintf(line2, sizeof(line2), STR_WIFI_IP_FORMAT, WiFi.softAPIP().toString().c_str());
      }
      else
      {
        snprintf(line2, sizeof(line2), "%s", apPassword.c_str());
      }
      displayUpdate(line1, line2);
    }
  }
}

void wifiManagerStopProvisioningAndSetManual(bool& provisioning)
{
  if (!provisioning)
    return;

  Serial.println("[SYSTEM] Exiting WiFi Setup -> Handbetrieb (Manual Mode)");
  webServerStop();
  dnsServer.stop();

  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);

  provisioning = false;
  displayUpdate(STR_READY, STR_MANUAL_MODE);
}

bool wifiManagerIsConnected()
{
  return WiFi.status() == WL_CONNECTED;
}

String wifiManagerGetSSID()
{
  return WiFi.SSID();
}

String wifiManagerGetLocalIP()
{
  return WiFi.localIP().toString();
}
