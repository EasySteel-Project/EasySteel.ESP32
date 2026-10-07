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
#include <ESPAsyncWebServer.h>
#include <WebResponseImpl.h>
#include "web_transport.h"
#include "web_template.h"
#include "web_body.h"
#include "browser_update.h"
#include <ArduinoOTA.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <stdarg.h>
#include "watchdog.h"

static AsyncWebServer server(80);
static WebRequest *activeRequest = nullptr; // Loop task only.
static WebRequestQueue<std::shared_ptr<WebRequest>, 4> requests;
static std::mutex requestsMutex;
static std::atomic<bool> serving{false};
static std::atomic<bool> apMode{false};
static std::atomic<uint32_t> generation{0};
static bool routesRegistered = false;
static bool wpsPending = false;
static String adminRealm;
static String adminPassword; // Initialized before the server first starts, then immutable.
static String *configuredName = nullptr;
static String *configuredServerIp = nullptr;
static uint16_t *configuredPort = nullptr;
static String apDeviceName;
static unsigned long restartAt = 0;
static bool scanRequested = false;
static bool scanRunning = false;
static String scanResults = "[]";

// Browser OTA is explicitly reserved by the loop before any flash write is allowed.
static BrowserUpdateSession browserUpdate;
static std::mutex updateMutex;
static std::atomic<bool> maintenanceActive{false};

struct StateSnapshot
{
  State state = STANDBY;
  unsigned long capturedAt = 0;
  unsigned long commandEpoch = 0;
  unsigned long timeMs = 0;
  unsigned long countdownId = 0;
  unsigned long startInMs = 0;
  int prepDelay = 0;
  unsigned long prepInMs = 0;
  int mode = 0;
  int targetCount = 0;
  int penaltyCount = 0;
  int autoMissCount = 0;
  bool hitMask[MAX_TARGETS] = {};
  int targetIds[MAX_RECORDED_HITS] = {};
  unsigned long splitTimes[MAX_RECORDED_HITS] = {};
  int hitCount = 0;
  TrainingResult trainingResult = {};
  bool hasTrainingResult = false;
  RemoteControlState remoteState = RemoteControlState::Disabled;
  bool pcConnected = false;
  char token[REMOTE_SESSION_TOKEN_LENGTH + 1] = "";
};
static std::mutex snapshotMutex;
static StateSnapshot stateSnapshot;
static char activityToken[REMOTE_SESSION_TOKEN_LENGTH + 1] = "";
static unsigned long activityAt = 0;
static bool activityPending = false;

void webServerPublishState()
{
  StateSnapshot snapshot;
  snapshot.capturedAt = millis();
  snapshot.commandEpoch = remoteControlGetCommandEpoch();
  snapshot.state = roundControllerGetState();
  snapshot.timeMs = roundControllerGetElapsedMs(snapshot.capturedAt);
  snapshot.countdownId = roundControllerGetCountdownId();
  snapshot.startInMs = roundControllerGetStartDelayRemainingMs(snapshot.capturedAt);
  snapshot.prepDelay = roundControllerGetPreparationDelay();
  snapshot.prepInMs = roundControllerGetPrepDelayRemainingMs(snapshot.capturedAt);
  snapshot.mode = roundControllerGetRoundMode();
  snapshot.targetCount = roundControllerGetTargetCount();
  snapshot.penaltyCount = roundControllerGetPenaltyCount();
  snapshot.autoMissCount = roundControllerGetAutomaticMissCount();
  roundControllerGetHitMask(snapshot.hitMask, MAX_TARGETS);
  snapshot.hitCount = roundControllerGetRecordedHits(snapshot.targetIds, snapshot.splitTimes, MAX_RECORDED_HITS);
  snapshot.hasTrainingResult = roundControllerGetLatestTrainingResult(snapshot.trainingResult);
  snapshot.remoteState = remoteControlGetState();
  snapshot.pcConnected = remoteControlIsPcConnected();
  remoteControlCopySessionToken(snapshot.token, sizeof(snapshot.token));
  std::unique_lock<std::mutex> lock(snapshotMutex, std::try_to_lock);
  if (lock.owns_lock()) { stateSnapshot = snapshot; }
}

static void scheduleRestart()
{
  restartAt = millis() + 2000;
}

struct TemplateVar {
  const char* key;   // Without % delimiters (e.g. "DEVICE_NAME")
  String value;
};

static void sendTemplate(const char *tpl, const TemplateVar *vars, size_t varCount)
{
  std::vector<WebTemplateValue> values;
  values.reserve(varCount);
  for (size_t index = 0; index < varCount; ++index)
  {
    values.push_back({vars[index].key, vars[index].value.c_str()});
  }
  std::shared_ptr<WebTemplate> stream = std::make_shared<WebTemplate>(tpl, std::move(values));
  activeRequest->response.reset(new AsyncChunkedResponse("text/html",
    [stream](uint8_t *buffer, size_t maxLength, size_t) { return stream->read(buffer, maxLength); }));
  activeRequest->response->addHeader("Cache-Control", "no-store");
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
  if (!activeRequest)
  {
    return;
  }

  char response[192];
  const char *format = includeSuccess
    ? "{\"success\":false,\"error\":\"%s\"}"
    : "{\"error\":\"%s\"}";
  snprintf(response, sizeof(response), format, message);
  activeRequest->send(statusCode, "application/json", response);
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
    {"TRAINING_MISS_ABBREVIATION", STR_TRAINING_MISS_ABBREVIATION},
    {"TRAINING_DISCONNECT", STR_TRAINING_DISCONNECT},
    {"WEB_CONNECTION_INTERRUPTED", STR_WEB_CONNECTION_INTERRUPTED},
    {"WEB_API_COMMAND_BUSY", STR_WEB_API_COMMAND_BUSY},
    {"TRAINING_PREP_DELAY", STR_TRAINING_PREP_DELAY},
    {"TRAINING_PREP_DELAY_HINT", STR_TRAINING_PREP_DELAY_HINT},
    {"TRAINING_PREP_DELAY_OFF", STR_TRAINING_PREP_DELAY_OFF},
    {"TRAINING_PREPARE", STR_TRAINING_PREPARE}
  };
  sendTemplate(TRAINING_PAGE, vars, sizeof(vars) / sizeof(vars[0]));
}

static void handleResetWifi()
{
  if (!activeRequest) return;



  networkResetCredentials();

  sendSuccessPage(STR_WEB_WIFI_RESET_MESSAGE);

  scheduleRestart();
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
  if (!activeRequest) return;

  Serial.println("[HTTP] POST /save-wifi - saving credentials");
  String ssid = activeRequest->arg("ssid");
  String pass = activeRequest->arg("pass");

  if (ssid.length() == 0)
  {
    activeRequest->send(400, "text/plain", STR_WEB_SSID_REQUIRED);
    return;
  }

  storageSaveWiFi(ssid, pass);

  sendSuccessPage(STR_WEB_WIFI_RESTART_MESSAGE);

  Serial.println("WiFi credentials saved. Restarting...");
  scheduleRestart();
}

static void handleWPS()
{
  if (!activeRequest) return;

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
    {"WEB_PREP_DELAY", STR_WEB_PREP_DELAY},
    {"PREP_DELAY", String(roundControllerGetPreparationDelay())},
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
  if (!activeRequest) return;


  String name = activeRequest->arg("name");
  String srvIp = activeRequest->arg("serverIp");
  String portStr = activeRequest->arg("port");
  IPAddress address;
  if (srvIp.length() > 0 && !address.fromString(srvIp))
  {
    sendJsonError(400, STR_WEB_API_INVALID_SERVER_IP, true);
    return;
  }

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
  String delayStr = activeRequest->arg("delay");
  if (delayStr.length() > 0)
  {
    int delayVal = delayStr.toInt();
    if (delayVal >= 0 && delayVal <= 30)
    {
      roundControllerSetPreparationDelay(delayVal);
    }
  }

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

static void handleApiState(AsyncWebServerRequest *request)
{


  unsigned long currentMillis = millis();
  StateSnapshot snapshot;
  String token = request->header("X-EasySteel-Token");
  bool isController;
  {
    std::lock_guard<std::mutex> lock(snapshotMutex);
    snapshot = stateSnapshot;
    isController = snapshot.remoteState == RemoteControlState::Paired &&
      token.length() == REMOTE_SESSION_TOKEN_LENGTH && token == snapshot.token &&
      currentMillis - snapshot.capturedAt < 1000 && !maintenanceActive.load();
    if (isController)
    {
      memcpy(activityToken, snapshot.token, sizeof(activityToken));
      activityAt = currentMillis;
      activityPending = true;
    }
  }
  State state = snapshot.state;
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

  RemoteControlState remoteState = snapshot.remoteState;
  unsigned long timeMs = snapshot.timeMs;
  unsigned long countdownId = snapshot.countdownId;
  unsigned long startInMs = snapshot.startInMs;
  int prepDelay = snapshot.prepDelay;
  unsigned long prepInMs = snapshot.prepInMs;
  int mode = snapshot.mode;
  int targetCount = snapshot.targetCount;
  int penaltyCount = snapshot.penaltyCount;
  int autoMissCount = snapshot.autoMissCount;

  const bool *hitMask = snapshot.hitMask;

  const int *targetIds = snapshot.targetIds;
  const unsigned long *splitTimes = snapshot.splitTimes;
  int hitCount = snapshot.hitCount;

  char jsonBuf[2048];
  int offset = 0;
  bool complete = appendJson(
    jsonBuf,
    sizeof(jsonBuf),
    offset,
    "{\"state\":\"%s\",\"timeMs\":%lu,\"countdownId\":%lu,\"startInMs\":%lu,\"prepDelay\":%d,\"prepInMs\":%lu,\"mode\":%d,\"targetCount\":%d,\"penaltyCount\":%d,\"missCount\":%d,\"remoteState\":\"%s\",\"isController\":%s,\"pcConnected\":%s,\"hitMask\":[%s,%s,%s,%s,%s],\"hits\":[",
    stateStr,
    timeMs,
    countdownId,
    startInMs,
    prepDelay,
    prepInMs,
    mode,
    targetCount,
    penaltyCount,
    autoMissCount,
    remoteControlStateName(remoteState),
    isController ? "true" : "false",
    snapshot.pcConnected ? "true" : "false",
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
  const TrainingResult &trainingResult = snapshot.trainingResult;
  if (complete && isController && snapshot.hasTrainingResult)
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
    request->send(500, "application/json", "{\"success\":false}");
    return;
  }
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonBuf);
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
}

static void handleApiPair()
{
  if (!activeRequest) return;

  String body = activeRequest->arg("plain");
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
    activeRequest->send(200, "application/json", response);
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
  if (!activeRequest) return;


  String token = activeRequest->header("X-EasySteel-Token");
  unsigned long currentMillis = millis();
  if (!remoteControlAuthorize(token.c_str(), currentMillis))
  {
    sendJsonError(401, STR_WEB_API_PHONE_NOT_PAIRED, true);
    return;
  }

  String body = activeRequest->arg("plain");
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
  else if (action == "delay")
  {
    remoteAction = RemoteControlAction::SetPrepDelay;
    actionValue = req["delay"] | -1;
  }
  else if (action == "disconnect")
  {
    remoteControlDisable();
    webServerPublishState();
    activeRequest->send(200, "application/json", "{\"success\":true}");
    return;
  }
  else
  {
    recognized = false;
  }

  if (!recognized || activeRequest->commandEpoch != remoteControlGetCommandEpoch() ||
      !remoteControlCommandStillValid(remoteAction, activeRequest->expectedState, state,
        activeRequest->countdownId, roundControllerGetCountdownId(), actionValue))
  {
    sendJsonError(409, STR_WEB_API_ACTION_NOT_ALLOWED, true);
    return;
  }

  if (!remoteControlQueueCommand(remoteAction, actionValue, currentMillis, state, roundControllerGetCountdownId()))
  {
    sendJsonError(409, STR_WEB_API_COMMAND_BUSY, true);
    return;
  }
  activeRequest->send(202, "application/json", "{\"success\":true}");
}

static bool isConfigurationRoute(WebRoute route)
{
  return route == WebRoute::SaveSettings || route == WebRoute::ResetWifi ||
    route == WebRoute::SaveWifi || route == WebRoute::Wps || route == WebRoute::PrepareUpdate;
}

static bool authenticated(AsyncWebServerRequest *request)
{
  if (request->authenticate("admin", adminPassword.c_str())) { return true; }
  request->requestAuthentication(adminRealm.c_str(), false);
  return false;
}

static void receiveBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  if (total == 0 || total > 512 || index > total || len > total - index)
  {
    request->send(413, "application/json", "{\"success\":false}");
    return;
  }
  if (index == 0)
  {
    void *storage = malloc(sizeof(WebBody));
    if (storage != nullptr) { request->_tempObject = new (storage) WebBody(); }
  }
  WebBody *body = static_cast<WebBody *>(request->_tempObject);
  if (body == nullptr || !body->append(data, len, index, total))
  {
    request->send(400, "application/json", "{\"success\":false}");
    return;
  }
}

static String parameter(AsyncWebServerRequest *request, const char *name)
{
  const AsyncWebParameter *value = request->getParam(name, true);
  return value == nullptr ? String() : value->value();
}

static void enqueueRequest(AsyncWebServerRequest *request, WebRoute route, bool admin)
{
  if (request->isSent()) { return; }
  if (!serving.load()) { request->send(503); return; }
  if (admin && !apMode.load() && !authenticated(request)) { return; }
  if (route == WebRoute::State) { handleApiState(request); return; }
  if (request->contentLength() > 512) { request->send(413); return; }
  if (request->hasHeader("X-EasySteel-Token") && request->header("X-EasySteel-Token").length() > REMOTE_SESSION_TOKEN_LENGTH)
  {
    request->send(401); return;
  }
  if (WebRequest::liveCount.load() >= 4) { request->send(503); return; }
  std::shared_ptr<WebRequest> job = std::make_shared<WebRequest>();
  job->route = route;
  job->queuedAt = millis();
  job->generation = generation.load();
  job->token = request->header("X-EasySteel-Token");
  if (route == WebRoute::Action)
  {
    std::lock_guard<std::mutex> lock(snapshotMutex);
    job->expectedState = stateSnapshot.state;
    job->countdownId = stateSnapshot.countdownId;
    job->commandEpoch = stateSnapshot.commandEpoch;
  }
  WebBody *body = static_cast<WebBody *>(request->_tempObject);
  if (route == WebRoute::Pair || route == WebRoute::Action)
  {
    if (body == nullptr || body->size != request->contentLength()) { request->send(400); return; }
    job->body = body->bytes;
  }
  job->name = parameter(request, "name");
  job->serverIp = parameter(request, "serverIp");
  job->port = parameter(request, "port");
  job->ssid = parameter(request, "ssid");
  job->password = parameter(request, "pass");
  job->delay = parameter(request, "delay");
  bool queued;
  {
    std::lock_guard<std::mutex> lock(requestsMutex);
    queued = requests.push(job);
  }
  if (!queued) { request->send(503, "application/json", "{\"success\":false}"); return; }
  request->send(new DeferredWebResponse(job));
}

static void finishUpload(AsyncWebServerRequest *request)
{
  if (!authenticated(request)) { return; }
  uint32_t lease = strtoul(request->header("X-EasySteel-Update").c_str(), nullptr, 10);
  bool success = request->_tempObject != nullptr && lease != 0 && lease == browserUpdate.lease() && browserUpdate.state() == BrowserUpdateState::Success;
  request->send(success ? 200 : 409, "text/plain", success ? "OK" : "FAIL");
}

static void uploadFirmware(AsyncWebServerRequest *request, const String &, size_t index, uint8_t *data, size_t len, bool final)
{
  if (!serving.load() || apMode.load() || !request->authenticate("admin", adminPassword.c_str())) { return; }
  uint32_t lease = strtoul(request->header("X-EasySteel-Update").c_str(), nullptr, 10);
  std::lock_guard<std::mutex> lock(updateMutex);
  if (lease == 0 || lease != browserUpdate.lease()) { return; }
  if (index == 0)
  {
    if (!browserUpdate.claim(lease, millis())) { return; }
    request->_tempObject = malloc(sizeof(uint32_t));
    if (request->_tempObject == nullptr) { browserUpdate.finish(false); return; }
    *static_cast<uint32_t *>(request->_tempObject) = lease;
    request->onDisconnect([lease]() {
      std::lock_guard<std::mutex> guard(updateMutex);
      if (lease == browserUpdate.lease() && browserUpdate.state() == BrowserUpdateState::Uploading)
      {
        Update.abort();
        browserUpdate.finish(false);
      }
    });
    if (!Update.begin(UPDATE_SIZE_UNKNOWN))
    {
      browserUpdate.finish(false);
      return;
    }
  }
  if (request->_tempObject == nullptr || *static_cast<uint32_t *>(request->_tempObject) != lease ||
      browserUpdate.state() != BrowserUpdateState::Uploading) { return; }
  if (!browserUpdate.acceptChunk(lease, index, len, millis()) || Update.write(data, len) != len)
  {
    Update.abort();
    browserUpdate.finish(false);
    return;
  }
  if (final)
  {
    browserUpdate.finish(Update.end(true));
  }
}

// Reject large bodies before the library parses form fields into heap Strings.
class BodyLimitHandler : public AsyncWebHandler
{
public:
  bool canHandle(AsyncWebServerRequest *request) const override
  {
    return request->url() != "/update" && request->contentLength() > 512;
  }
  void handleRequest(AsyncWebServerRequest *request) override { request->send(413); }
  bool isRequestHandlerTrivial() const override { return true; }
};

static void registerRoutes()
{
  if (routesRegistered) { return; }
  routesRegistered = true;
  server.addHandler(new BodyLimitHandler());
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    enqueueRequest(request, apMode.load() ? WebRoute::WifiSetup : WebRoute::Training, false);
  });
  struct Route { const char *path; WebRequestMethod method; WebRoute route; bool admin; bool apOnly; };
  const Route routes[] = {
    {"/training", HTTP_GET, WebRoute::Training, false, false},
    {"/api/state", HTTP_GET, WebRoute::State, false, false},
    {"/api/pair", HTTP_POST, WebRoute::Pair, false, false},
    {"/api/action", HTTP_POST, WebRoute::Action, false, false},
    {"/settings", HTTP_GET, WebRoute::Settings, true, false},
    {"/save-settings", HTTP_POST, WebRoute::SaveSettings, true, false},
    {"/reset-wifi", HTTP_POST, WebRoute::ResetWifi, true, false},
    {"/save-wifi", HTTP_POST, WebRoute::SaveWifi, false, true},
    {"/scan-wifi", HTTP_GET, WebRoute::ScanWifi, false, true},
    {"/wps", HTTP_GET, WebRoute::Wps, false, true},
    {"/update/prepare", HTTP_POST, WebRoute::PrepareUpdate, true, false}
  };
  for (const Route &route : routes)
  {
    server.on(route.path, route.method, [route](AsyncWebServerRequest *request) {
      bool accessPoint = apMode.load();
      if (route.apOnly != accessPoint && route.route != WebRoute::ResetWifi)
      {
        request->send(404); return;
      }
      enqueueRequest(request, route.route, route.admin);
    }, nullptr, receiveBody);
  }
  server.on("/update", HTTP_POST, finishUpload, uploadFirmware);
  server.onNotFound([](AsyncWebServerRequest *request) {
    if (apMode.load()) { request->redirect("/"); }
    else { request->send(404); }
  });
}

void webServerStop()
{
  serving.store(false);
  generation.fetch_add(1);
  server.end();
}

bool webServerCheckAndClearWpsPending()
{
  bool pending = wpsPending;
  wpsPending = false;
  return pending;
}

void webServerInitAP(const String &deviceName, const String &apPassword)
{
  webServerStop();
  remoteControlDisable();
  if (!routesRegistered) { adminPassword = apPassword; adminRealm = STR_APP_NAME; }
  apDeviceName = deviceName;
  apMode.store(true);
  scanRequested = true;
  registerRoutes();
  webServerPublishState();
  serving.store(true);
  server.begin();
}

void webServerInitSTA(String &deviceName, String &serverIP, uint16_t &tcpPort, const String &apPassword)
{
  webServerStop();
  if (!routesRegistered) { adminPassword = apPassword; adminRealm = STR_APP_NAME; }
  configuredName = &deviceName;
  configuredServerIp = &serverIP;
  configuredPort = &tcpPort;
  apMode.store(false);
  registerRoutes();
  webServerPublishState();
  serving.store(true);
  server.begin();
  ArduinoOTA.setHostname(deviceName.c_str());
  ArduinoOTA.setPassword(apPassword.c_str());
  ArduinoOTA.onStart([]() {
    maintenanceActive.store(true);
    remoteControlDisable();
    watchdogDisable();
    sensorIsrDisable();
    WiFi.setSleep(false);
    displayUpdate(STR_WEB_OTA_DISPLAY_TITLE, STR_WEB_OTA_FLASHING);
  });
  ArduinoOTA.onEnd([]() { watchdogEnable(); maintenanceActive.store(false); });
  ArduinoOTA.onProgress([](unsigned int, unsigned int) { watchdogFeed(); });
  ArduinoOTA.onError([](ota_error_t) {
    watchdogEnable();
    maintenanceActive.store(false);
    displayUpdate(STR_WEB_OTA_ERROR_TITLE, STR_WEB_OTA_ABORT);
  });
  ArduinoOTA.begin();
}

bool webServerMaintenanceActive()
{
  return maintenanceActive.load() || restartAt != 0;
}

static void scanTick()
{
  if (!apMode.load()) { return; }
  if (scanRequested && !scanRunning)
  {
    scanRequested = false;
    if (WiFi.getMode() != WIFI_AP_STA) { WiFi.mode(WIFI_AP_STA); }
    scanRunning = WiFi.scanNetworks(true, false) == WIFI_SCAN_RUNNING;
  }
  if (!scanRunning) { return; }
  int count = WiFi.scanComplete();
  if (count == WIFI_SCAN_RUNNING) { return; }
  JsonDocument result;
  JsonArray entries = result.to<JsonArray>();
  for (int index = 0; index < count && index < 24; ++index)
  {
    JsonObject entry = entries.add<JsonObject>();
    entry["ssid"] = WiFi.SSID(index);
    entry["rssi"] = WiFi.RSSI(index);
  }
  scanResults = "";
  serializeJson(result, scanResults);
  WiFi.scanDelete();
  scanRunning = false;
}

static void updateTick()
{
  BrowserUpdateState state = browserUpdate.state();
  if (state == BrowserUpdateState::Idle) { return; }
  std::unique_lock<std::mutex> lock(updateMutex, std::try_to_lock);
  if (!lock.owns_lock()) { return; }
  state = browserUpdate.state();
  if (browserUpdate.expired(millis()))
  {
    if (state == BrowserUpdateState::Uploading) { Update.abort(); }
    browserUpdate.finish(false);
    state = BrowserUpdateState::Failed;
  }
  if (state == BrowserUpdateState::Success)
  {
    if (restartAt == 0) {
      watchdogEnable();
      maintenanceActive.store(false);
      scheduleRestart();
    }
  }
  else if (state == BrowserUpdateState::Failed)
  {
    Update.abort();
    browserUpdate.reset();
    watchdogEnable();
    maintenanceActive.store(false);
    displayUpdate(STR_WEB_OTA_ERROR_TITLE, STR_WEB_OTA_ABORT);
  }
}

void webServerTick()
{
  remoteControlSetPcConnected(networkIsServerConnected());
  char token[REMOTE_SESSION_TOKEN_LENGTH + 1] = "";
  unsigned long touchedAt = 0;
  {
    std::unique_lock<std::mutex> lock(snapshotMutex, std::try_to_lock);
    if (lock.owns_lock() && activityPending)
    {
      memcpy(token, activityToken, sizeof(token));
      touchedAt = activityAt;
      activityPending = false;
    }
  }
  if (token[0] != '\0' && millis() - touchedAt < 1000) { remoteControlAuthorize(token, millis()); }
  remoteControlTick(millis());
  updateTick();
  if (restartAt != 0 && static_cast<int32_t>(millis() - restartAt) >= 0) { ESP.restart(); }
  scanTick();

  std::shared_ptr<WebRequest> job;
  {
    std::unique_lock<std::mutex> lock(requestsMutex, std::try_to_lock);
    if (lock.owns_lock()) { requests.pop(job); }
  }
  if (job && !job->cancelled.load())
  {
    activeRequest = job.get();
    if (job->generation != generation.load() || webRequestExpired(millis(), job->queuedAt))
    {
      sendJsonError(503, STR_WEB_API_COMMAND_BUSY, true);
    }
    else if (webServerMaintenanceActive() ||
      (isConfigurationRoute(job->route) && !apMode.load() && !roundControllerIsIdle()))
    {
      sendJsonError(423, STR_WEB_OTA_IDLE_REQUIRED, true);
    }
    else
    {
      switch (job->route)
      {
        case WebRoute::Training: handleTraining(); break;
        case WebRoute::State: break;
        case WebRoute::Pair: handleApiPair(); break;
        case WebRoute::Action: handleApiAction(); break;
        case WebRoute::Settings: handleSettings(*configuredName, *configuredServerIp, *configuredPort); break;
        case WebRoute::SaveSettings: handleSaveSettings(*configuredName, *configuredServerIp, *configuredPort); break;
        case WebRoute::ResetWifi: handleResetWifi(); break;
        case WebRoute::WifiSetup: handleWifiSetup(apDeviceName); break;
        case WebRoute::SaveWifi: handleSaveWifi(); break;
        case WebRoute::Wps: handleWPS(); break;
        case WebRoute::ScanWifi:
          if (scanRunning || scanRequested) { activeRequest->send(202, "application/json", "[]"); }
          else { activeRequest->send(200, "application/json", scanResults); scanRequested = true; }
          break;
        case WebRoute::PrepareUpdate:
        {
          std::unique_lock<std::mutex> lock(updateMutex, std::try_to_lock);
          if (!lock.owns_lock()) { sendJsonError(503, STR_WEB_API_COMMAND_BUSY, true); break; }
          remoteControlDisable();
          sensorIsrDisable();
          WiFi.setSleep(false);
          watchdogDisable();
          maintenanceActive.store(true);
          uint32_t lease = esp_random();
          if (lease == 0) { lease = 1; }
          if (!browserUpdate.reserve(lease, millis(), roundControllerIsIdle()))
          {
            maintenanceActive.store(false);
            watchdogEnable();
            sendJsonError(423, STR_WEB_OTA_IDLE_REQUIRED, true);
            break;
          }
          displayUpdate(STR_WEB_OTA_DISPLAY_TITLE, STR_WEB_OTA_FLASHING);
          activeRequest->send(200, "application/json", "{\"lease\":\"" + String(lease) + "\"}");
          break;
        }
      }
    }
    if (!job->response) { job->send(500, "application/json", "{\"success\":false}"); }
    job->ready.store(true);
    activeRequest = nullptr;
  }
  if (serving.load() && !apMode.load() && !webServerMaintenanceActive() && roundControllerIsIdle())
  {
    ArduinoOTA.handle();
  }
}
