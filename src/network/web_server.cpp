#include "web_server.h"
#include "web_pages.h"
#include "training_page.h"
#include "remote_control.h"
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
#include <ArduinoJson.h>
#include <stdarg.h>
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
    {"MESSAGE", "Settings saved.<br><a href='/settings' style='color:#4ecca3'>Back to Settings</a>"}
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

// --- REST API Handlers for the paired solo-training dashboard ---

static bool appendJson(char *buffer, size_t bufferSize, int &offset, const char *format, ...)
{
  if (offset < 0 || static_cast<size_t>(offset) >= bufferSize)
  {
    return false;
  }

  va_list args;
  va_start(args, format);
  int written = vsnprintf(buffer + offset, bufferSize - static_cast<size_t>(offset), format, args);
  va_end(args);
  if (written < 0 || static_cast<size_t>(written) >= bufferSize - static_cast<size_t>(offset))
  {
    buffer[bufferSize - 1] = '\0';
    return false;
  }

  offset += written;
  return true;
}

static const char *remoteControlStateName(RemoteControlState state)
{
  switch (state)
  {
    case RemoteControlState::Pairing: return "pairing";
    case RemoteControlState::Paired: return "paired";
    case RemoteControlState::BlockedByPc: return "blocked";
    case RemoteControlState::Disabled: return "disabled";
  }
  return "disabled";
}

static void handleApiState()
{
  if (!httpServer) return;

  unsigned long currentMillis = millis();
  State state = roundControllerGetState();
  const char* stateStr = "STANDBY";
  switch (state)
  {
    case STANDBY: stateStr = "STANDBY"; break;
    case DELAY: stateStr = "DELAY"; break;
    case RUNNING: stateStr = "RUNNING"; break;
    case FINISHED: stateStr = "FINISHED"; break;
    case MENU: stateStr = "MENU"; break;
    case FALSE_START: stateStr = "FALSE_START"; break;
  }

  String token = httpServer->header("X-EasySteel-Token");
  bool isController = remoteControlAuthorize(token.c_str(), currentMillis);
  RemoteControlState remoteState = remoteControlGetState();
  unsigned long timeMs = roundControllerGetElapsedMs(currentMillis);
  int mode = roundControllerGetRoundMode();
  int targetCount = roundControllerGetTargetCount();
  int penaltyCount = roundControllerGetPenaltyCount();
  int autoMissCount = roundControllerGetAutomaticMissCount();

  bool hitMask[MAX_TARGETS] = {false};
  roundControllerGetHitMask(hitMask, MAX_TARGETS);

  int targetIds[MAX_RECORDED_HITS];
  unsigned long splitTimes[MAX_RECORDED_HITS];
  int hitCount = roundControllerGetRecordedHits(targetIds, splitTimes, MAX_RECORDED_HITS);

  char jsonBuf[2048];
  int offset = 0;
  bool complete = appendJson(
    jsonBuf,
    sizeof(jsonBuf),
    offset,
    "{\"state\":\"%s\",\"timeMs\":%lu,\"mode\":%d,\"targetCount\":%d,\"penaltyCount\":%d,\"missCount\":%d,\"remoteState\":\"%s\",\"isController\":%s,\"pcConnected\":%s,\"hitMask\":[%s,%s,%s,%s,%s],\"hits\":[",
    stateStr,
    timeMs,
    mode,
    targetCount,
    penaltyCount,
    autoMissCount,
    remoteControlStateName(remoteState),
    isController ? "true" : "false",
    remoteControlIsPcConnected() ? "true" : "false",
    hitMask[0] ? "true" : "false",
    hitMask[1] ? "true" : "false",
    hitMask[2] ? "true" : "false",
    hitMask[3] ? "true" : "false",
    hitMask[4] ? "true" : "false"
  );

  for (int i = 0; complete && i < hitCount; i++)
  {
    complete = appendJson(jsonBuf, sizeof(jsonBuf), offset,
      "%s{\"target\":%d,\"splitMs\":%lu}",
      (i > 0 ? "," : ""),
      targetIds[i],
      splitTimes[i]
    );
  }

  complete = complete && appendJson(jsonBuf, sizeof(jsonBuf), offset, "],\"trainingResult\":");
  TrainingResult trainingResult = {};
  if (complete && isController && roundControllerGetLatestTrainingResult(trainingResult))
  {
    complete = appendJson(
      jsonBuf,
      sizeof(jsonBuf),
      offset,
      "{\"id\":\"%s\",\"rawTimeMs\":%lu,\"timeMs\":%lu,\"mode\":%d,\"targetCount\":%d,\"missCount\":%d,\"penaltyCount\":%d,\"hitMask\":[%s,%s,%s,%s,%s],\"hits\":[",
      trainingResult.id,
      trainingResult.rawTimeMs,
      trainingResult.timeMs,
      trainingResult.mode,
      trainingResult.targetCount,
      trainingResult.missCount,
      trainingResult.penaltyCount,
      trainingResult.hitMask[0] ? "true" : "false",
      trainingResult.hitMask[1] ? "true" : "false",
      trainingResult.hitMask[2] ? "true" : "false",
      trainingResult.hitMask[3] ? "true" : "false",
      trainingResult.hitMask[4] ? "true" : "false");
    for (int hitIndex = 0; complete && hitIndex < trainingResult.hitCount; hitIndex++)
    {
      complete = appendJson(
        jsonBuf,
        sizeof(jsonBuf),
        offset,
        "%s{\"target\":%d,\"splitMs\":%lu}",
        hitIndex > 0 ? "," : "",
        trainingResult.hits[hitIndex].targetId,
        trainingResult.hits[hitIndex].splitTimeMs);
    }
    complete = complete && appendJson(jsonBuf, sizeof(jsonBuf), offset, "]}");
  }
  else if (complete)
  {
    complete = appendJson(jsonBuf, sizeof(jsonBuf), offset, "null");
  }
  complete = complete && appendJson(jsonBuf, sizeof(jsonBuf), offset, "}");

  if (!complete)
  {
    httpServer->send(500, "application/json", "{\"error\":\"State response overflow\"}");
    return;
  }
  httpServer->send(200, "application/json", jsonBuf);
}

static void handleApiPair()
{
  if (!httpServer) return;

  String body = httpServer->arg("plain");
  JsonDocument request;
  DeserializationError error = deserializeJson(request, body);
  if (error)
  {
    httpServer->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
    return;
  }

  const char *pin = request["pin"] | "";
  char token[REMOTE_SESSION_TOKEN_LENGTH + 1] = "";
  RemotePairResult result = remoteControlPair(pin, millis(), token, sizeof(token));
  if (result == RemotePairResult::Success)
  {
    char response[96];
    snprintf(response, sizeof(response), "{\"success\":true,\"token\":\"%s\"}", token);
    httpServer->send(200, "application/json", response);
    return;
  }
  if (result == RemotePairResult::AttemptsExceeded)
  {
    httpServer->send(429, "application/json", "{\"success\":false,\"error\":\"Pairing closed after five attempts\"}");
    return;
  }
  if (result == RemotePairResult::InvalidPin)
  {
    httpServer->send(403, "application/json", "{\"success\":false,\"error\":\"Invalid PIN\"}");
    return;
  }
  httpServer->send(409, "application/json", "{\"success\":false,\"error\":\"Pairing is not active\"}");
}

static void handleApiAction()
{
  if (!httpServer) return;

  if (httpServer->method() != HTTP_POST)
  {
    httpServer->send(405, "application/json", "{\"success\":false,\"error\":\"Method Not Allowed\"}");
    return;
  }

  String token = httpServer->header("X-EasySteel-Token");
  unsigned long currentMillis = millis();
  if (!remoteControlAuthorize(token.c_str(), currentMillis))
  {
    httpServer->send(401, "application/json", "{\"success\":false,\"error\":\"Phone is not paired\"}");
    return;
  }

  String body = httpServer->arg("plain");
  JsonDocument req;
  DeserializationError error = deserializeJson(req, body);
  if (error)
  {
    httpServer->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
    return;
  }

  String action = req["action"] | "";
  State state = roundControllerGetState();
  RemoteControlAction remoteAction = RemoteControlAction::Start;
  int actionValue = 0;
  bool recognized = true;

  if (action == "start")
  {
    remoteAction = RemoteControlAction::Start;
  }
  else if (action == "stop")
  {
    remoteAction = RemoteControlAction::Stop;
  }
  else if (action == "reset")
  {
    remoteAction = RemoteControlAction::Reset;
  }
  else if (action == "mode")
  {
    remoteAction = RemoteControlAction::SetMode;
    actionValue = req["mode"] | -1;
  }
  else
  {
    recognized = false;
  }

  if (!recognized || !remoteControlActionAllowed(remoteAction, state, actionValue))
  {
    httpServer->send(409, "application/json", "{\"success\":false,\"error\":\"Action is not allowed in the current state\"}");
    return;
  }

  if (!remoteControlQueueCommand(remoteAction, actionValue))
  {
    httpServer->send(409, "application/json", "{\"success\":false,\"error\":\"Command slot is busy\"}");
    return;
  }
  httpServer->send(202, "application/json", "{\"success\":true}");
}

static void registerCommonApiRoutes()
{
  if (!httpServer) return;
  httpServer->on("/api/state", HTTP_GET, handleApiState);
  httpServer->on("/api/pair", HTTP_POST, handleApiPair);
  httpServer->on("/api/action", HTTP_POST, handleApiAction);
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
  remoteControlDisable();
  adminAuthRequired = false;
  adminPassword = apPassword;

  httpServer = new WebServer(80);
  httpServer->on("/", HTTP_GET, [deviceName]() { handleWifiSetup(deviceName); });
  httpServer->on("/scan-wifi", HTTP_GET, handleScanWifi);
  httpServer->on("/save-wifi", HTTP_POST, handleSaveWifi);
  httpServer->on("/wps", HTTP_GET, handleWPS);
  httpServer->on("/reset-wifi", HTTP_POST, handleResetWifi);

  // Captive Portal redirect: Send unknown paths to setup
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
  const char *headerKeys[] = {"X-EasySteel-Token"};
  httpServer->collectHeaders(headerKeys, 1);

  // Serve Solo Training Dashboard on root and /training
  httpServer->on("/", HTTP_GET, []() {
    if (httpServer) httpServer->send(200, "text/html", TRAINING_PAGE);
  });
  httpServer->on("/training", HTTP_GET, []() {
    if (httpServer) httpServer->send(200, "text/html", TRAINING_PAGE);
  });

  // Settings page
  httpServer->on("/settings", HTTP_GET, [&deviceName, &serverIP, &tcpPort]() { handleSettings(deviceName, serverIP, tcpPort); });
  httpServer->on("/save-settings", HTTP_POST, [&deviceName, &serverIP, &tcpPort]() { handleSaveSettings(deviceName, serverIP, tcpPort); });
  httpServer->on("/reset-wifi", HTTP_POST, handleResetWifi);

  registerCommonApiRoutes();
  registerUpdateHandler();
  httpServer->begin();

  Serial.println("Solo Training Dashboard at http://" + WiFi.localIP().toString() + "/");
  Serial.println("Settings page at http://" + WiFi.localIP().toString() + "/settings");

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
  unsigned long currentMillis = millis();
  remoteControlSetPcConnected(networkIsServerConnected());
  remoteControlTick(currentMillis);

  if (httpServer)
  {
    httpServer->handleClient();
  }
  if (roundControllerIsIdle())
  {
    ArduinoOTA.handle();
  }
}
