#include "tcp_client.h"
#include "offline_buffer.h"
#include "tcp_outgoing_queue.h"
#include "tcp_protocol.h"
#include "round_controller.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <lwip/sockets.h>
#include <cerrno>

static WiFiClient tcpClient;
static unsigned long lastHeartbeatMs = 0;
static constexpr unsigned long HEARTBEAT_INTERVAL_MS = 10000;

static unsigned long lastConnectAttemptMs = 0;
static constexpr unsigned long CONNECT_RETRY_INTERVAL_MS = 15000;
static constexpr unsigned long CAPABILITY_NEGOTIATION_MS = 1000;

enum class ResultAckMode { Unknown, Legacy, Acknowledged };
static ResultAckMode resultAckMode = ResultAckMode::Unknown;
static unsigned long connectedAtMs = 0;
static uint32_t bootSessionId = 0;
static uint32_t nextRunNumber = 1;
static char receiveBuffer[192] = {};
static size_t receiveLength = 0;

static void createRunId(char *runId, size_t capacity)
{
  snprintf(runId, capacity, "%08lx-%lu", static_cast<unsigned long>(bootSessionId),
           static_cast<unsigned long>(nextRunNumber++));
}

static void resetPendingTransmission()
{
  tcpOutgoingQueueClear();
  offlineBufferMarkAllUnsent();
}

static bool enqueueEvent(
  const String& chipId,
  const String& deviceName,
  const char *event,
  int target,
  unsigned long timeMs,
  int missedTargets,
  TcpMessageKind kind,
  const char *runId = nullptr)
{
  char payload[TCP_EVENT_MAX_BYTES] = {};
  const size_t length = tcpProtocolSerializeEvent(
    payload, sizeof(payload), chipId, deviceName, event, target, timeMs,
    roundControllerGetTargetCount(), missedTargets, runId);
  if (length == 0)
  {
    Serial.println("[TCP] Event exceeds the fixed protocol buffer");
    return false;
  }

  return tcpOutgoingQueuePush(payload, length, kind, runId);
}

static void handleServerMessage(const char *json)
{
  // ArduinoJson 7 deprecates the fixed-capacity alias, but it deliberately avoids heap use here.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  StaticJsonDocument<256> doc;
#pragma GCC diagnostic pop
  if (deserializeJson(doc, json) != DeserializationError::Ok)
    return;

  const char *event = doc["event"] | "";
  if (strcmp(event, "capabilities") == 0)
  {
    resultAckMode = doc["resultAck"] | false
      ? ResultAckMode::Acknowledged
      : ResultAckMode::Legacy;
    return;
  }

  const char *acknowledgedRunId = doc["ack"] | nullptr;
  if (acknowledgedRunId && offlineBufferAcknowledge(acknowledgedRunId))
  {
    Serial.print("[OUTBOX] Server acknowledged run ");
    Serial.println(acknowledgedRunId);
  }
}

static void receiveServerMessages()
{
  while (tcpClient.available() > 0)
  {
    const char value = static_cast<char>(tcpClient.read());
    if (value == '\n')
    {
      receiveBuffer[receiveLength] = '\0';
      handleServerMessage(receiveBuffer);
      receiveLength = 0;
    }
    else if (value != '\r')
    {
      if (receiveLength < sizeof(receiveBuffer) - 1)
        receiveBuffer[receiveLength++] = value;
      else
        receiveLength = 0;
    }
  }
}

static void queueOldestResult(const String& chipId, const String& deviceName)
{
  if (resultAckMode == ResultAckMode::Unknown)
    return;

  BufferedRunData run = {};
  if (!offlineBufferPeekOldestUnsent(run))
    return;

  const TcpMessageKind kind = resultAckMode == ResultAckMode::Acknowledged
    ? TcpMessageKind::AcknowledgedResult
    : TcpMessageKind::LegacyResult;
  if (enqueueEvent(
        chipId, deviceName, "end", -1, run.timeMs, run.missedTargets,
        kind, run.runId))
  {
    offlineBufferMarkOldestQueued(run.runId);
  }
}

static void handleCompletedMessage(const TcpOutgoingCompletion& completion)
{
  if (completion.kind == TcpMessageKind::LegacyResult)
    offlineBufferCompleteOldestSend(completion.runId, false);
  else if (completion.kind == TcpMessageKind::AcknowledgedResult)
    offlineBufferCompleteOldestSend(completion.runId, true);
}

static void flushOneNonBlockingWrite()
{
  TcpOutgoingView view = {};
  if (!tcpOutgoingQueuePeek(view))
    return;

  const int socketFd = tcpClient.fd();
  if (socketFd < 0)
    return;

  const int sent = lwip_send(socketFd, view.data, view.length, MSG_DONTWAIT);
  if (sent > 0)
  {
    TcpOutgoingCompletion completion = {};
    if (tcpOutgoingQueueConsume(static_cast<size_t>(sent), completion))
      handleCompletedMessage(completion);
    return;
  }

  if (sent == 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
  {
    Serial.println("[TCP] Non-blocking send failed; reconnecting");
    tcpClient.stop();
    resetPendingTransmission();
  }
}

void tcpClientInit()
{
  lastHeartbeatMs = 0;
  lastConnectAttemptMs = 0;
  bootSessionId = esp_random();
  nextRunNumber = 1;
  receiveLength = 0;
  resetPendingTransmission();
}

bool tcpClientIsConnected()
{
  return tcpClient && tcpClient.connected();
}

void tcpClientDisconnect()
{
  if (tcpClient)
    tcpClient.stop();
  resetPendingTransmission();
}

void tcpClientSendEvent(
  const String& chipId,
  const String& deviceName,
  bool provisioning,
  const char *event,
  int target,
  unsigned long timeMs,
  int missedTargets)
{
  if (provisioning)
    return;

  if (strcmp(event, "end") == 0)
  {
    char runId[24] = {};
    createRunId(runId, sizeof(runId));
    if (!offlineBufferPush(runId, timeMs, missedTargets))
      Serial.println("[OUTBOX] Warning: oldest result replaced because outbox is full");
    return;
  }

  if (!tcpClient || !tcpClient.connected())
    return;

  if (!enqueueEvent(
        chipId, deviceName, event, target, timeMs, missedTargets,
        TcpMessageKind::Live))
  {
    Serial.println("[TCP] Live event dropped because the outgoing queue is full");
  }
}

void tcpClientTick(
  unsigned long currentMillis,
  bool provisioning,
  const String& serverIP,
  uint16_t tcpPort,
  const String& chipId,
  const String& deviceName)
{
  if (provisioning || serverIP.length() == 0 || WiFi.status() != WL_CONNECTED)
    return;

  if (!tcpClient || !tcpClient.connected())
  {
    if (tcpClient)
    {
      Serial.println("Disconnected from server");
      tcpClient.stop();
    }
    resetPendingTransmission();

    if (currentMillis - lastConnectAttemptMs >= CONNECT_RETRY_INTERVAL_MS)
    {
      lastConnectAttemptMs = currentMillis;

      static unsigned long lastConnectLogMs = 0;
      const bool shouldLog = lastConnectLogMs == 0 || currentMillis - lastConnectLogMs >= 60000;
      if (shouldLog)
      {
        lastConnectLogMs = currentMillis;
        Serial.print("Connecting to server: ");
        Serial.print(serverIP);
        Serial.print(":");
        Serial.println(tcpPort);
      }

      if (tcpClient.connect(serverIP.c_str(), tcpPort, 150))
      {
        Serial.println("Connected to server!");
        resultAckMode = ResultAckMode::Unknown;
        connectedAtMs = currentMillis;
        receiveLength = 0;
        resetPendingTransmission();
        tcpClientSendEvent(chipId, deviceName, provisioning, "online", -1, 0, 0);
      }
      else if (shouldLog)
      {
        Serial.println("Connection failed");
      }
    }
    return;
  }

  receiveServerMessages();

  if (resultAckMode == ResultAckMode::Unknown &&
      currentMillis - connectedAtMs >= CAPABILITY_NEGOTIATION_MS)
  {
    resultAckMode = ResultAckMode::Legacy;
    Serial.println("[TCP] Server did not advertise result ACK support; using legacy delivery");
  }

  queueOldestResult(chipId, deviceName);

  if (currentMillis - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS)
  {
    lastHeartbeatMs = currentMillis;
    tcpClientSendEvent(chipId, deviceName, provisioning, "ping", -1, 0, 0);
  }

  flushOneNonBlockingWrite();
}
