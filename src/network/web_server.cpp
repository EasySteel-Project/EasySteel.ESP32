#include "web_server.h"
#include "web_pages.h"
#include "storage.h"
#include "display.h"
#include "tcp_client.h"
#include "network.h"
#include "sensor_isr.h"
#include "round_controller.h"

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include "watchdog.h"


static WebServer *httpServer = nullptr;
static bool wpsPending = false;
static bool adminAuthRequired = false;
static String adminPassword;
static bool browserOtaActive = false;
static bool browserOtaSuccessful = false;
static bool browserOtaRejected = false;

static bool requireAdminAuthentication()
{
  if (!httpServer || !adminAuthRequired)
    return true;

  if (httpServer->authenticate("admin", adminPassword.c_str()))
    return true;

  httpServer->requestAuthentication(BASIC_AUTH, "EasySteel", "Authentication required");
  return false;
}

static void finishBrowserOtaSession()
{
  if (browserOtaActive)
  {
    watchdogEnable();
    browserOtaActive = false;
  }
  WiFi.setSleep(false);
}

struct TemplateVar {
  const char* key;   // Without % delimiters (e.g. "DEVICE_NAME")
  String value;
};

static void sendTemplate(const char* tpl, const TemplateVar* vars, size_t varCount)
{
  if (!httpServer) return;

  httpServer->setContentLength(CONTENT_LENGTH_UNKNOWN);
  httpServer->send(200, "text/html", "");

  const char* pos = tpl;
  while (*pos)
  {
    const char* pct = strchr(pos, '%');
    if (!pct)
    {
      httpServer->sendContent(pos);
      break;
    }

    if (pct > pos)
    {
      httpServer->sendContent(String(pos, (unsigned int)(pct - pos)));
    }

    const char* keyStart = pct + 1;
    const char* endPct = strchr(keyStart, '%');
    if (endPct)
    {
      size_t keyLen = endPct - keyStart;
      bool matched = false;
      for (size_t i = 0; i < varCount; i++)
      {
        if (strlen(vars[i].key) == keyLen &&
            strncmp(keyStart, vars[i].key, keyLen) == 0)
        {
          if (vars[i].value.length() > 0)
          {
            httpServer->sendContent(vars[i].value);
          }
          pos = endPct + 1;
          matched = true;
          break;
        }
      }
      if (!matched)
      {
        httpServer->sendContent("%");
        pos = keyStart;
      }
    }
    else
    {
      httpServer->sendContent("%");
      pos = keyStart;
    }
  }

  httpServer->sendContent(""); // End chunked transfer
}


static void handleResetWifi()
{
  if (!httpServer) return;

  if (!requireAdminAuthentication())
    return;

  if (httpServer->method() != HTTP_POST)
  {
    httpServer->send(405, "text/plain", "Method Not Allowed");
    return;
  }

  networkResetCredentials();

  const TemplateVar vars[] = {
    {"MESSAGE", "WiFi reset. Restarting into setup mode..."}
  };
  sendTemplate(SUCCESS_PAGE, vars, 1);

  delay(2000);
  ESP.restart();
}

static void handleWifiSetup(const String& deviceName)
{
  Serial.println("[HTTP] GET / - serving WiFi setup page");
  const TemplateVar vars[] = {
    {"DEVICE_NAME", deviceName}
  };
  sendTemplate(WIFI_SETUP_PAGE, vars, 1);
}

static void handleSaveWifi()
{
  if (!httpServer) return;

  Serial.println("[HTTP] POST /save-wifi - saving credentials");
  String ssid = httpServer->arg("ssid");
  String pass = httpServer->arg("pass");

  if (ssid.length() == 0)
  {
    httpServer->send(400, "text/plain", "SSID required");
    return;
  }

  storageSaveWiFi(ssid, pass);

  const TemplateVar vars[] = {
    {"MESSAGE", "Restarting device...<br>Connect to your WiFi and look for the device."}
  };
  sendTemplate(SUCCESS_PAGE, vars, 1);

  Serial.println("WiFi credentials saved. Restarting...");
  delay(2000);
  ESP.restart();
}

static void handleWPS()
{
  if (!httpServer) return;

  httpServer->send(200, "text/html", WPS_PAGE);
  wpsPending = true;
}

static void handleSettings(const String& deviceName, const String& serverIP, uint16_t tcpPort)
{
  if (!requireAdminAuthentication())
    return;

  const TemplateVar vars[] = {
    {"DEVICE_NAME", deviceName},
    {"SERVER_IP", serverIP},
    {"WIFI_SSID", WiFi.SSID()},
    {"DEVICE_IP", WiFi.localIP().toString()},
    {"TCP_PORT", String(tcpPort)}
  };
  sendTemplate(SETTINGS_PAGE, vars, 5);
}

static void handleSaveSettings(String& deviceName, String& serverIP, uint16_t& tcpPort)
{
  if (!httpServer) return;

  if (!requireAdminAuthentication())
    return;

  String name = httpServer->arg("name");
  String srvIp = httpServer->arg("serverIp");
  String portStr = httpServer->arg("port");

  if (name.length() == 0)
    name = deviceName;

  uint16_t port = portStr.length() > 0 ? (uint16_t)portStr.toInt() : DEFAULT_TCP_PORT;
  if (port == 0)
    port = DEFAULT_TCP_PORT;

  bool portChanged = (port != tcpPort);
  bool ipChanged = (srvIp != serverIP);

  deviceName = name;
  serverIP = srvIp;
  tcpPort = port;
  storageSaveDevice(name, srvIp, port);

  networkSendEvent("settings_changed", -1, 0);

  const TemplateVar vars[] = {
    {"MESSAGE", "Settings saved.<br><a href='/' style='color:#4ecca3'>Back to Settings</a>"}
  };
  sendTemplate(SUCCESS_PAGE, vars, 1);

  Serial.print("Device name: ");
  Serial.println(name);
  Serial.print("Server IP: ");
  Serial.println(srvIp);
  Serial.print("TCP Port: ");
  Serial.println(port);

  if ((portChanged || ipChanged))
  {
    Serial.println("TCP server settings changed. Disconnecting existing socket...");
    tcpClientDisconnect();
  }
}

void webServerStop()
{
  if (httpServer)
  {
    httpServer->stop();
    delete httpServer;
    httpServer = nullptr;
  }
}

bool webServerCheckAndClearWpsPending()
{
  if (wpsPending)
  {
    wpsPending = false;
    return true;
  }
  return false;
}

static void handleScanWifi()
{
  if (!httpServer) return;

  if (WiFi.getMode() != WIFI_AP_STA && WiFi.getMode() != WIFI_MODE_APSTA)
  {
    WiFi.mode(WIFI_AP_STA);
    delay(50);
  }

  int n = WiFi.scanNetworks(false, false);
  if (n < 0)
  {
    // Retry once if busy
    delay(100);
    n = WiFi.scanNetworks(false, false);
  }

  Serial.printf("[WIFI] Scan found %d networks\n", n);

  String json = "[";
  if (n > 0)
  {
    bool first = true;
    for (int i = 0; i < n; i++)
    {
      String ssid = WiFi.SSID(i);
      if (ssid.length() == 0) continue;

      ssid.replace("\\", "\\\\");
      ssid.replace("\"", "\\\"");

      if (!first) json += ",";
      first = false;

      json += "{\"ssid\":\"";
      json += ssid;
      json += "\",\"rssi\":";
      json += WiFi.RSSI(i);
      json += "}";
    }
  }
  json += "]";
  WiFi.scanDelete();
  httpServer->send(200, "application/json", json);
}


static void registerUpdateHandler()
{
  if (!httpServer) return;

  httpServer->on("/update", HTTP_POST, []() {
    if (!requireAdminAuthentication())
      return;

    if (browserOtaRejected)
    {
      httpServer->send(423, "text/plain", "Device must be idle before firmware update");
      browserOtaRejected = false;
      return;
    }

    bool success = browserOtaSuccessful && !Update.hasError();
    httpServer->sendHeader("Connection", "close");
    httpServer->send(200, "text/plain", success ? "OK" : "FAIL");
    if (success)
    {
      Serial.println("[OTA] Browser update completed successfully. Restarting...");
      displayUpdate("OTA UPDATE", "Erfolgreich!");
      delay(1000);
      ESP.restart();
    }
    else
    {
      Serial.printf("[OTA] Browser update failed: error %u\n", Update.getError());
      displayUpdate("OTA FEHLER", "Abbruch");
    }
  }, []() {
    HTTPUpload& upload = httpServer->upload();
    if (upload.status == UPLOAD_FILE_START)
    {
      browserOtaSuccessful = false;
      browserOtaRejected = false;

      if (!requireAdminAuthentication() || !roundControllerIsIdle())
      {
        browserOtaRejected = true;
        return;
      }

      watchdogDisable();
      sensorIsrDisable();
      WiFi.setSleep(false);
      browserOtaActive = true;
      Serial.printf("[OTA] Browser Upload Start: %s\n", upload.filename.c_str());
      displayUpdate("OTA BROWSER", "Flashe...");

      if (!Update.begin(UPDATE_SIZE_UNKNOWN))
      {
        Update.printError(Serial);
        finishBrowserOtaSession();
      }
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
      if (!browserOtaActive)
        return;

      watchdogFeed();
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
      {
        Update.printError(Serial);
        Update.abort();
        finishBrowserOtaSession();
      }
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
      if (!browserOtaActive)
        return;

      browserOtaSuccessful = Update.end(true);
      if (browserOtaSuccessful)
      {
        Serial.printf("[OTA] Browser Upload Success: %u bytes\n", upload.totalSize);
      }
      else
      {
        Update.printError(Serial);
      }
      finishBrowserOtaSession();
    }
    else if (upload.status == UPLOAD_FILE_ABORTED)
    {
      if (browserOtaActive)
      {
        Update.abort();
      }
      finishBrowserOtaSession();
      Serial.println("[OTA] Browser Upload Aborted");
    }
  });
}

void webServerInitAP(const String& deviceName, const String& apPassword)
{
  webServerStop();
  adminAuthRequired = false;
  adminPassword = apPassword;

  httpServer = new WebServer(80);
  httpServer->on("/", HTTP_GET, [deviceName]() { handleWifiSetup(deviceName); });
  httpServer->on("/scan-wifi", HTTP_GET, handleScanWifi);
  httpServer->on("/save-wifi", HTTP_POST, handleSaveWifi);
  httpServer->on("/wps", HTTP_GET, handleWPS);
  httpServer->on("/reset-wifi", HTTP_POST, handleResetWifi);

  // Captive Portal redirect: Send any unknown paths to the root "/" setup page
  httpServer->onNotFound([]() {
    if (httpServer) {
      Serial.printf("[HTTP] 404 Redirect: %s -> /\n", httpServer->uri().c_str());
      httpServer->sendHeader("Location", "/", true);
      httpServer->send(302, "text/plain", "");
    }
  });

  httpServer->begin();
}

void webServerInitSTA(String& deviceName, String& serverIP, uint16_t& tcpPort, const String& apPassword)
{
  webServerStop();
  adminAuthRequired = true;
  adminPassword = apPassword;

  httpServer = new WebServer(80);
  httpServer->on("/", HTTP_GET, [&deviceName, &serverIP, &tcpPort]() { handleSettings(deviceName, serverIP, tcpPort); });
  httpServer->on("/save-settings", HTTP_POST, [&deviceName, &serverIP, &tcpPort]() { handleSaveSettings(deviceName, serverIP, tcpPort); });
  httpServer->on("/reset-wifi", HTTP_POST, handleResetWifi);
  registerUpdateHandler();
  httpServer->begin();

  Serial.println("Settings page at http://" + WiFi.localIP().toString() + "/");


  ArduinoOTA.setHostname(deviceName.c_str());
  if (apPassword.length() > 0)
  {
    ArduinoOTA.setPassword(apPassword.c_str());
  }

  ArduinoOTA.onStart([]() {
    watchdogDisable();
    sensorIsrDisable();
    WiFi.setSleep(false);
    Serial.println("OTA Start");
    displayUpdate("OTA UPDATE", "Flashe...");
  });
  ArduinoOTA.onEnd([]() {
    watchdogEnable();
    Serial.println("\nOTA End");
    displayUpdate("OTA UPDATE", "Erfolgreich!");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    watchdogFeed();
    if (total > 0)
    {
      Serial.printf("Progress: %u%%\r", (unsigned int)(progress * 100ULL / total));
    }
  });
  ArduinoOTA.onError([](ota_error_t error) {
    watchdogEnable();
    Serial.printf("Error[%u]: ", error);
    displayUpdate("OTA FEHLER", "Abbruch");
  });

  ArduinoOTA.begin();
  Serial.println("OTA initialized");
}

void webServerTick()
{
  if (httpServer)
  {
    httpServer->handleClient();
  }
  if (roundControllerIsIdle())
  {
    ArduinoOTA.handle();
  }
}
