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
#include "ui_strings.h"

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

  httpServer->requestAuthentication(BASIC_AUTH, STR_APP_NAME, STR_WEB_AUTHENTICATION_REQUIRED);
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

static void sendSuccessPage(const char *message)
{
  const TemplateVar vars[] = {
    {"LANG", i18nGetLanguageCode()},
    {"WEB_SUCCESS_PAGE_TITLE", STR_WEB_SUCCESS_PAGE_TITLE},
    {"WEB_SUCCESS_HEADING", STR_WEB_SUCCESS_HEADING},
    {"MESSAGE", message}
  };
  sendTemplate(SUCCESS_PAGE, vars, sizeof(vars) / sizeof(vars[0]));
}

static void sendJsonError(int statusCode, const char *message, bool includeSuccess)
{
  if (!httpServer)
  {
    return;
  }

  char response[192];
  const char *format = includeSuccess
    ? "{\"success\":false,\"error\":\"%s\"}"
    : "{\"error\":\"%s\"}";
  snprintf(response, sizeof(response), format, message);
  httpServer->send(statusCode, "application/json", response);
}

static void handleTraining()
{
  const TemplateVar vars[] = {
    {"LANG", i18nGetLanguageCode()},
    {"TRAINING_PAGE_TITLE", STR_TRAINING_PAGE_TITLE},
    {"APP_NAME", STR_APP_NAME},
    {"TRAINING_WAKE_OFF", STR_TRAINING_WAKE_OFF},
    {"TRAINING_WAKE_ON", STR_TRAINING_WAKE_ON},
    {"TRAINING_BEEP_OFF", STR_TRAINING_BEEP_OFF},
    {"TRAINING_BEEP_ON", STR_TRAINING_BEEP_ON},
    {"TRAINING_PC_LOCKED", STR_TRAINING_PC_LOCKED},
    {"TRAINING_RELEASE_REQUIRED", STR_TRAINING_RELEASE_REQUIRED},
    {"TRAINING_PIN_PLACEHOLDER", STR_TRAINING_PIN_PLACEHOLDER},
    {"TRAINING_PAIR", STR_TRAINING_PAIR},
    {"TRAINING_STANDBY", STR_PREPARE},
    {"TRAINING_TARGET_PREFIX", STR_TRAINING_TARGET_PREFIX},
    {"TRAINING_STOP_PLATE", STR_STOP_PLATE},
    {"TRAINING_MODE_COMPETITION", STR_MODE_COMPETITION},
    {"TRAINING_MODE_ALL_TARGETS", STR_MODE_ALL_TARGETS},
    {"TRAINING_MODE_DRILL", STR_MODE_TAG_DRILL},
    {"TRAINING_START", STR_TRAINING_START},
    {"TRAINING_STOP", STR_TRAINING_STOP},
    {"TRAINING_RESET_ACTION", STR_RESET},
    {"TRAINING_COUNTDOWN_START", STR_TRAINING_COUNTDOWN_START},
    {"TRAINING_ROUND_STOP", STR_TRAINING_ROUND_STOP},
    {"TRAINING_RESET", STR_TRAINING_RESET},
    {"TRAINING_CURRENT_SPLITS", STR_TRAINING_CURRENT_SPLITS},
    {"TRAINING_NO_HITS", STR_TRAINING_NO_HITS},
    {"TRAINING_STAGE", STR_TRAINING_STAGE},
    {"TRAINING_LOCAL_ONLY", STR_TRAINING_LOCAL_ONLY},
    {"TRAINING_STAGE_5_TO_GO", STR_TRAINING_STAGE_5_TO_GO},
    {"TRAINING_STAGE_SHOWDOWN", STR_TRAINING_STAGE_SHOWDOWN},
    {"TRAINING_STAGE_SMOKE_AND_HOPE", STR_TRAINING_STAGE_SMOKE_AND_HOPE},
    {"TRAINING_STAGE_OUTER_LIMITS", STR_TRAINING_STAGE_OUTER_LIMITS},
    {"TRAINING_STAGE_ACCELERATOR", STR_TRAINING_STAGE_ACCELERATOR},
    {"TRAINING_STAGE_THE_PENDULUM", STR_TRAINING_STAGE_THE_PENDULUM},
    {"TRAINING_STAGE_SPEED_OPTION", STR_TRAINING_STAGE_SPEED_OPTION},
    {"TRAINING_STAGE_ROUNDABOUT", STR_TRAINING_STAGE_ROUNDABOUT},
    {"TRAINING_HISTORY_LOCAL", STR_TRAINING_HISTORY_LOCAL},
    {"TRAINING_RUNS", STR_TRAINING_RUNS},
    {"TRAINING_ALL", STR_TRAINING_ALL},
    {"TRAINING_BEST_TIME", STR_TRAINING_BEST_TIME},
    {"TRAINING_AVERAGE", STR_TRAINING_AVERAGE},
    {"TRAINING_NO_RUNS_SAVED", STR_TRAINING_NO_RUNS_SAVED},
    {"TRAINING_COPY_CSV", STR_TRAINING_COPY_CSV},
    {"TRAINING_DELETE_HISTORY", STR_TRAINING_DELETE_HISTORY},
    {"TRAINING_SETTINGS_LINK", STR_TRAINING_SETTINGS_LINK},
    {"TRAINING_UNKNOWN", STR_TRAINING_UNKNOWN},
    {"TRAINING_NO_RUNS_CATEGORY", STR_TRAINING_NO_RUNS_CATEGORY},
    {"TRAINING_NO_ENTRIES", STR_TRAINING_NO_ENTRIES},
    {"TRAINING_CSV_HEADER", STR_TRAINING_CSV_HEADER},
    {"TRAINING_CSV_COPIED", STR_TRAINING_CSV_COPIED},
    {"TRAINING_CSV_PROMPT", STR_TRAINING_CSV_PROMPT},
    {"TRAINING_CLEAR_CONFIRM", STR_TRAINING_CLEAR_CONFIRM},
    {"TRAINING_PIN_INVALID", STR_TRAINING_PIN_INVALID},
    {"TRAINING_PAIR_FAILED", STR_TRAINING_PAIR_FAILED},
    {"TRAINING_CONTROLLER", STR_TRAINING_CONTROLLER},
    {"TRAINING_PAIR_PROMPT", STR_TRAINING_PAIR_PROMPT},
    {"TRAINING_OTHER_PAIRED", STR_TRAINING_OTHER_PAIRED},
    {"TRAINING_PC_PRIORITY", STR_TRAINING_PC_PRIORITY},
    {"TRAINING_RELEASE_MAIN_MENU", STR_TRAINING_RELEASE_MAIN_MENU},
    {"TRAINING_NEXT_RUN", STR_TRAINING_NEXT_RUN},
    {"TRAINING_ABORT_COUNTDOWN", STR_TRAINING_ABORT_COUNTDOWN},
    {"TRAINING_BACK_TO_STANDBY", STR_TRAINING_BACK_TO_STANDBY},
    {"TRAINING_DISCARD_RUN", STR_TRAINING_DISCARD_RUN},
    {"TRAINING_PENALTY_LABEL", STR_TRAINING_PENALTY_LABEL},
    {"TRAINING_TARGET_LABEL", STR_TRAINING_TARGET_LABEL},
    {"TRAINING_SECONDS_UNIT", STR_UNIT_SECONDS},
    {"TRAINING_MISS_ABBREVIATION", STR_TRAINING_MISS_ABBREVIATION}
  };
  sendTemplate(TRAINING_PAGE, vars, sizeof(vars) / sizeof(vars[0]));
}

static void handleResetWifi()
{
  if (!httpServer) return;

  if (!requireAdminAuthentication())
    return;

  if (httpServer->method() != HTTP_POST)
  {
    httpServer->send(405, "text/plain", STR_WEB_METHOD_NOT_ALLOWED);
    return;
  }

  networkResetCredentials();

  sendSuccessPage(STR_WEB_WIFI_RESET_MESSAGE);

  delay(2000);
  ESP.restart();
}

static void handleWifiSetup(const String& deviceName)
{
  Serial.println("[HTTP] GET / - serving WiFi setup page");
  const TemplateVar vars[] = {
    {"LANG", i18nGetLanguageCode()},
    {"WEB_WIFI_SETUP_TITLE", STR_WEB_WIFI_SETUP_TITLE},
    {"WEB_WIFI_SETUP_HEADING", STR_WEB_WIFI_SETUP_HEADING},
    {"WEB_WIFI_SETUP_SUBTITLE", STR_WEB_WIFI_SETUP_SUBTITLE},
    {"WEB_AVAILABLE_NETWORKS", STR_WEB_AVAILABLE_NETWORKS},
    {"WEB_RESCAN", STR_WEB_RESCAN},
    {"WEB_SCANNING_NETWORKS", STR_WEB_SCANNING_NETWORKS},
    {"WEB_WIFI_NAME", STR_WEB_WIFI_NAME},
    {"WEB_NETWORK_NAME", STR_WEB_NETWORK_NAME},
    {"WEB_PASSWORD", STR_WEB_PASSWORD},
    {"WEB_WIFI_PASSWORD", STR_WEB_WIFI_PASSWORD},
    {"WEB_SAVE_AND_CONNECT", STR_WEB_SAVE_AND_CONNECT},
    {"WEB_OR", STR_WEB_OR},
    {"WEB_WPS_HINT", STR_WEB_WPS_HINT},
    {"WEB_WPS_START", STR_WEB_WPS_START},
    {"DEVICE_NAME", deviceName},
    {"WEB_NO_NETWORKS", STR_WEB_NO_NETWORKS},
    {"WEB_SELECT_NETWORK_PREFIX", STR_WEB_SELECT_NETWORK_PREFIX},
    {"WEB_SELECT_NETWORK_SUFFIX", STR_WEB_SELECT_NETWORK_SUFFIX},
    {"WEB_MANUAL_ENTRY", STR_WEB_MANUAL_ENTRY},
    {"WEB_SIGNAL_UNIT", STR_WEB_SIGNAL_UNIT}
  };
  sendTemplate(WIFI_SETUP_PAGE, vars, sizeof(vars) / sizeof(vars[0]));
}

static void handleSaveWifi()
{
  if (!httpServer) return;

  Serial.println("[HTTP] POST /save-wifi - saving credentials");
  String ssid = httpServer->arg("ssid");
  String pass = httpServer->arg("pass");

  if (ssid.length() == 0)
  {
    httpServer->send(400, "text/plain", STR_WEB_SSID_REQUIRED);
    return;
  }

  storageSaveWiFi(ssid, pass);

  sendSuccessPage(STR_WEB_WIFI_RESTART_MESSAGE);

  Serial.println("WiFi credentials saved. Restarting...");
  delay(2000);
  ESP.restart();
}

static void handleWPS()
{
  if (!httpServer) return;

  const TemplateVar vars[] = {
    {"LANG", i18nGetLanguageCode()},
    {"WEB_WPS_PAGE_TITLE", STR_WEB_WPS_PAGE_TITLE},
    {"WEB_WPS_ACTIVE", STR_WEB_WPS_ACTIVE},
    {"WEB_WPS_WAITING", STR_WEB_WPS_WAITING}
  };
  sendTemplate(WPS_PAGE, vars, sizeof(vars) / sizeof(vars[0]));
  wpsPending = true;
}

static void handleSettings(const String& deviceName, const String& serverIP, uint16_t tcpPort)
{
  if (!requireAdminAuthentication())
    return;

  const TemplateVar vars[] = {
    {"LANG", i18nGetLanguageCode()},
    {"WEB_SETTINGS_TITLE", STR_WEB_SETTINGS_TITLE},
    {"WEB_SETTINGS_HEADING", STR_WEB_SETTINGS_HEADING},
    {"WEB_SETTINGS_SUBTITLE", STR_WEB_SETTINGS_SUBTITLE},
    {"WEB_DEVICE_NAME", STR_WEB_DEVICE_NAME},
    {"DEVICE_NAME", deviceName},
    {"WEB_SERVER_IP", STR_SERVER_IP},
    {"SERVER_IP", serverIP},
    {"WEB_SERVER_IP_PLACEHOLDER", STR_WEB_SERVER_IP_PLACEHOLDER},
    {"WEB_TCP_PORT", STR_PORT},
    {"WEB_SAVE_SETTINGS", STR_WEB_SAVE_SETTINGS},
    {"WEB_SSID", STR_SSID},
    {"WIFI_SSID", WiFi.SSID()},
    {"WEB_IP", STR_WEB_IP},
    {"DEVICE_IP", WiFi.localIP().toString()},
    {"WEB_PORT", STR_WEB_PORT},
    {"TCP_PORT", String(tcpPort)},
    {"WEB_FIRMWARE_UPDATE", STR_WEB_FIRMWARE_UPDATE},
    {"WEB_FLASH_FIRMWARE", STR_WEB_FLASH_FIRMWARE},
    {"WEB_RESET_WIFI_CONFIRM", STR_WEB_RESET_WIFI_CONFIRM},
    {"WEB_RESET_WIFI", STR_WEB_RESET_WIFI},
    {"WEB_INSTALL_FIRMWARE_PREFIX", STR_WEB_INSTALL_FIRMWARE_PREFIX},
    {"WEB_INSTALL_FIRMWARE_SUFFIX", STR_WEB_INSTALL_FIRMWARE_SUFFIX},
    {"WEB_UPLOAD_RUNNING", STR_WEB_UPLOAD_RUNNING},
    {"WEB_FLASHING_PREFIX", STR_WEB_FLASHING_PREFIX},
    {"WEB_UPDATE_SUCCESS", STR_WEB_UPDATE_SUCCESS},
    {"WEB_ERROR_PREFIX", STR_WEB_ERROR_PREFIX},
    {"WEB_CONNECTION_INTERRUPTED", STR_WEB_CONNECTION_INTERRUPTED},
    {"WEB_PERCENT_UNIT", STR_WEB_PERCENT_UNIT}
  };
  sendTemplate(SETTINGS_PAGE, vars, sizeof(vars) / sizeof(vars[0]));
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

  sendSuccessPage(STR_WEB_SETTINGS_SAVED_MESSAGE);

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
  unsigned long countdownId = roundControllerGetCountdownId();
  unsigned long startInMs = roundControllerGetStartDelayRemainingMs(currentMillis);
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
    "{\"state\":\"%s\",\"timeMs\":%lu,\"countdownId\":%lu,\"startInMs\":%lu,\"mode\":%d,\"targetCount\":%d,\"penaltyCount\":%d,\"missCount\":%d,\"remoteState\":\"%s\",\"isController\":%s,\"pcConnected\":%s,\"hitMask\":[%s,%s,%s,%s,%s],\"hits\":[",
    stateStr,
    timeMs,
    countdownId,
    startInMs,
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
    sendJsonError(500, STR_WEB_API_STATE_OVERFLOW, false);
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
    sendJsonError(400, STR_WEB_API_INVALID_JSON, true);
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
    sendJsonError(429, STR_WEB_API_PAIRING_ATTEMPTS, true);
    return;
  }
  if (result == RemotePairResult::InvalidPin)
  {
    sendJsonError(403, STR_WEB_API_INVALID_PIN, true);
    return;
  }
  sendJsonError(409, STR_WEB_API_PAIRING_INACTIVE, true);
}

static void handleApiAction()
{
  if (!httpServer) return;

  if (httpServer->method() != HTTP_POST)
  {
    sendJsonError(405, STR_WEB_METHOD_NOT_ALLOWED, true);
    return;
  }

  String token = httpServer->header("X-EasySteel-Token");
  unsigned long currentMillis = millis();
  if (!remoteControlAuthorize(token.c_str(), currentMillis))
  {
    sendJsonError(401, STR_WEB_API_PHONE_NOT_PAIRED, true);
    return;
  }

  String body = httpServer->arg("plain");
  JsonDocument req;
  DeserializationError error = deserializeJson(req, body);
  if (error)
  {
    sendJsonError(400, STR_WEB_API_INVALID_JSON, true);
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
    sendJsonError(409, STR_WEB_API_ACTION_NOT_ALLOWED, true);
    return;
  }

  if (!remoteControlQueueCommand(remoteAction, actionValue))
  {
    sendJsonError(409, STR_WEB_API_COMMAND_BUSY, true);
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
      httpServer->send(423, "text/plain", STR_WEB_OTA_IDLE_REQUIRED);
      browserOtaRejected = false;
      return;
    }

    bool success = browserOtaSuccessful && !Update.hasError();
    httpServer->sendHeader("Connection", "close");
    httpServer->send(200, "text/plain", success ? "OK" : "FAIL");
    if (success)
    {
      Serial.println("[OTA] Browser update completed successfully. Restarting...");
      displayUpdate(STR_WEB_OTA_DISPLAY_TITLE, STR_WEB_OTA_SUCCESS);
      delay(1000);
      ESP.restart();
    }
    else
    {
      Serial.printf("[OTA] Browser update failed: error %u\n", Update.getError());
      displayUpdate(STR_WEB_OTA_ERROR_TITLE, STR_WEB_OTA_ABORT);
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
      displayUpdate(STR_WEB_OTA_BROWSER_TITLE, STR_WEB_OTA_FLASHING);

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
    handleTraining();
  });
  httpServer->on("/training", HTTP_GET, []() {
    handleTraining();
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
    displayUpdate(STR_WEB_OTA_DISPLAY_TITLE, STR_WEB_OTA_FLASHING);
  });
  ArduinoOTA.onEnd([]() {
    watchdogEnable();
    Serial.println("\nOTA End");
    displayUpdate(STR_WEB_OTA_DISPLAY_TITLE, STR_WEB_OTA_SUCCESS);
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
    displayUpdate(STR_WEB_OTA_ERROR_TITLE, STR_WEB_OTA_ABORT);
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
