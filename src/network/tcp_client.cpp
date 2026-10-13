#include "tcp_client.h"
#include "offline_buffer.h"
#include "tcp_outgoing_queue.h"
#include "tcp_protocol.h"
#include "tcp_line_framer.h"
#include "tcp_receive_budget.h"
#include "tcp_connect_attempt.h"
#include "round_controller.h"
#include <WiFi.h>
#include <esp_system.h>
#include <lwip/sockets.h>
#include <cerrno>
#include <fcntl.h>

struct ConnectSocket
{
  static int open(uint32_t address, uint16_t port)
  {
    const int fd = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { return -1; }
    const int flags = lwip_fcntl(fd, F_GETFL, 0);
    if (flags < 0 || lwip_fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
      close(fd);
      return -1;
    }
    sockaddr_in peer = {};
    peer.sin_family = AF_INET;
    peer.sin_addr.s_addr = address;
    peer.sin_port = htons(port);
    if (lwip_connect(fd, reinterpret_cast<sockaddr*>(&peer), sizeof(peer)) < 0 &&
        errno != EINPROGRESS)
    {
      close(fd);
      return -1;
    }
    return fd;
  }

  static int poll(int fd)
  {
    fd_set writable;
    FD_ZERO(&writable);
    FD_SET(fd, &writable);
    timeval timeout = {};
    const int ready = lwip_select(fd + 1, nullptr, &writable, nullptr, &timeout);
    if (ready <= 0) { return ready; }
    int error = 0;
    socklen_t length = sizeof(error);
    return lwip_getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) == 0 && error == 0
      ? 1 : -1;
  }

  static void close(int fd) { lwip_close(fd); }
};

static TcpConnectAttempt<ConnectSocket> connectAttempt;

static WiFiClient tcpClient;
static unsigned long lastConnectAttemptMs = 0;
static constexpr unsigned long CONNECT_RETRY_INTERVAL_MS = 3000;
static constexpr unsigned long CAPABILITY_NEGOTIATION_MS = 1000;
static constexpr unsigned long RESULT_ACK_TIMEOUT_MS = 3000;
static constexpr unsigned long SERVER_PING_TIMEOUT_MS = 30000;

enum class ProtocolSessionState { AwaitingCapabilities, Ready };
static ProtocolSessionState protocolSessionState = ProtocolSessionState::AwaitingCapabilities;
static unsigned long connectedAtMs = 0;
static unsigned long lastServerPingMs = 0;
static uint32_t lastServerPingSequence = 0;
static TcpLineFramer incomingFramer;

static void createRunId(char *runId, size_t capacity)
{
  snprintf(runId, capacity, "%08lx%08lx%08lx%08lx",
    static_cast<unsigned long>(esp_random()),
    static_cast<unsigned long>(esp_random()),
    static_cast<unsigned long>(esp_random()),
    static_cast<unsigned long>(esp_random()));
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
  int targetCount,
  TcpMessageKind kind,
  const char *runId = nullptr,
  unsigned long rawTimeMs = 0,
  int missCount = 0,
  int penaltyCount = 0,
  const char *contextId = nullptr,
  uint32_t sequence = 0)
{
  char payload[TCP_EVENT_MAX_BYTES] = {};
  const size_t length = tcpProtocolSerializeEvent(
    payload, sizeof(payload), chipId, deviceName, event, target, timeMs,
    targetCount, runId, rawTimeMs, missCount, penaltyCount,
    contextId, sequence);
  if (length == 0)
  {
    Serial.println("[TCP] Event exceeds the fixed protocol buffer");
    return false;
  }

  return tcpOutgoingQueuePush(payload, length, kind, runId);
}

static void handleServerMessage(
  const char *json,
  size_t length,
  unsigned long currentMillis,
  const String& chipId,
  const String& deviceName)
{
  TcpServerMessage message = {};
  if (!tcpProtocolParseServerMessage(json, length, message))
  {
    return;
  }

  if (message.type == TcpServerMessageType::Capabilities)
  {
    if (protocolSessionState == ProtocolSessionState::AwaitingCapabilities &&
        currentMillis - connectedAtMs < CAPABILITY_NEGOTIATION_MS)
    {
      protocolSessionState = ProtocolSessionState::Ready;
      Serial.println("[TCP] Protocol v1 capability negotiation completed");
      return;
    }

    Serial.println("[TCP] Unexpected capabilities message; reconnecting");
    tcpClient.stop();
    resetPendingTransmission();
    return;
  }

  if (protocolSessionState != ProtocolSessionState::Ready)
  {
    Serial.println("[TCP] Server message received before capabilities; reconnecting");
    tcpClient.stop();
    resetPendingTransmission();
    return;
  }

  if (message.type == TcpServerMessageType::Acknowledgement &&
      offlineBufferAcknowledge(message.runId))
  {
    Serial.print("[OUTBOX] Server acknowledged run ");
    Serial.println(message.runId);
    return;
  }

  if (message.type == TcpServerMessageType::MatchInfo &&
      roundControllerIsIdle())
  {
    roundControllerSetMatchInfo(message.contextId, message.stage);
  }

  if (message.type == TcpServerMessageType::Ping)
  {
    if (lastServerPingSequence != 0 &&
        static_cast<int32_t>(message.sequence - lastServerPingSequence) <= 0)
    {
      return;
    }

    lastServerPingSequence = message.sequence;
    lastServerPingMs = currentMillis;
    enqueueEvent(chipId, deviceName, "pong", -1, 0,
      roundControllerGetTargetCount(), TcpMessageKind::Control,
      nullptr, 0, 0, 0, nullptr, message.sequence);
  }
}

static void receiveServerMessages(
  unsigned long currentMillis,
  const String& chipId,
  const String& deviceName)
{
  tcpReceiveBudget(tcpClient, [&](char value) {

    const char *json = nullptr;
    size_t length = 0;
    const TcpLineFrameResult result = incomingFramer.push(value, json, length);
    if (result == TcpLineFrameResult::Complete)
    {
      handleServerMessage(json, length, currentMillis, chipId, deviceName);
    }
    else if (result == TcpLineFrameResult::Discarded)
    {
      Serial.println("[TCP] Oversized server frame discarded");
    }
  });
}

static void queueOldestResult(const String& chipId, const String& deviceName)
{
  if (protocolSessionState != ProtocolSessionState::Ready)
    return;

  BufferedRunData run = {};
  if (!offlineBufferPeekOldestUnsent(run))
    return;

  if (enqueueEvent(
        chipId, deviceName, "end", -1, run.timeMs, run.targetCount,
        TcpMessageKind::ConfirmedResult, run.runId,
        run.rawTimeMs, run.missCount, run.penaltyCount, run.contextId))
  {
    offlineBufferMarkOldestQueued(run.runId);
  }
}

static void handleCompletedMessage(
  const TcpOutgoingCompletion& completion,
  unsigned long currentMillis)
{
  if (completion.kind == TcpMessageKind::ConfirmedResult)
  {
    offlineBufferCompleteOldestSend(completion.runId, currentMillis);
  }
}

static void flushOneNonBlockingWrite(unsigned long currentMillis)
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
    {
      handleCompletedMessage(completion, currentMillis);
    }
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
  connectAttempt.cancel();
  lastConnectAttemptMs = 0;
  lastServerPingMs = 0;
  lastServerPingSequence = 0;
  incomingFramer.reset();
  resetPendingTransmission();
}

bool tcpClientIsConnected()
{
  return tcpClient && tcpClient.connected();
}

void tcpClientDisconnect()
{
  connectAttempt.cancel();
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
  unsigned long timeMs)
{
  if (provisioning)
    return;

  if (!tcpClient || !tcpClient.connected())
  {
    return;
  }

  if (!enqueueEvent(
        chipId, deviceName, event, target, timeMs,
        roundControllerGetTargetCount(), TcpMessageKind::Live))
  {
    Serial.println("[TCP] Live event dropped because the outgoing queue is full");
  }
}

bool tcpClientSendResult(
  bool provisioning,
  unsigned long rawTimeMs,
  unsigned long timeMs,
  int missCount,
  int penaltyCount,
  const char *contextId)
{
  if (provisioning || roundControllerGetState() != FINISHED)
  {
    return false;
  }

  char runId[TCP_RUN_ID_BYTES] = {};
  createRunId(runId, sizeof(runId));
  if (!offlineBufferPush(
        runId,
        contextId,
        rawTimeMs,
        timeMs,
        missCount,
        penaltyCount,
        roundControllerGetTargetCount()))
  {
    Serial.println("[OUTBOX] Result not accepted; keep the pending result on the device");
    return false;
  }
  return true;
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
  {
    connectAttempt.cancel();
    return;
  }

  if (!tcpClient || !tcpClient.connected())
  {
    if (tcpClient)
    {
      Serial.println("Disconnected from server");
      tcpClient.stop();
    }
    resetPendingTransmission();

    if (!roundControllerIsIdle())
    {
      connectAttempt.cancel();
      return;
    }

    if (connectAttempt.pending())
    {
      const int connectedFd = connectAttempt.poll(currentMillis, true);
      if (connectedFd >= 0)
      {
        // WiFiClient now owns the socket; reads and writes remain non-blocking.
        tcpClient = WiFiClient(connectedFd);
        Serial.println("Connected to server!");
        protocolSessionState = ProtocolSessionState::AwaitingCapabilities;
        connectedAtMs = currentMillis;
        lastServerPingMs = currentMillis;
        lastServerPingSequence = 0;
        incomingFramer.reset();
        resetPendingTransmission();
        tcpClientSendEvent(chipId, deviceName, provisioning, "online", -1, 0);
      }
      return;
    }

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

      IPAddress address;
      if (!address.fromString(serverIP))
      {
        if (shouldLog) { Serial.println("[TCP] Invalid server IP; DNS lookup intentionally disabled"); }
        return;
      }
      connectAttempt.begin(static_cast<uint32_t>(address), tcpPort, currentMillis);
      if (!connectAttempt.pending() && shouldLog)
      {
        Serial.println("Connection failed");
      }
    }
    return;
  }

  receiveServerMessages(currentMillis, chipId, deviceName);

  if (!tcpClient || !tcpClient.connected())
  {
    return;
  }

  if (protocolSessionState == ProtocolSessionState::AwaitingCapabilities &&
      currentMillis - connectedAtMs >= CAPABILITY_NEGOTIATION_MS)
  {
    Serial.println("[TCP] Required protocol v1 capabilities missing; reconnecting");
    tcpClient.stop();
    resetPendingTransmission();
    return;
  }

  if (protocolSessionState == ProtocolSessionState::Ready &&
      offlineBufferReleaseTimedOutAck(currentMillis, RESULT_ACK_TIMEOUT_MS))
  {
    Serial.println("[OUTBOX] ACK timeout; scheduling result retransmission");
  }

  if (protocolSessionState == ProtocolSessionState::Ready &&
      currentMillis - lastServerPingMs >= SERVER_PING_TIMEOUT_MS)
  {
    Serial.println("[TCP] Server ping timeout; reconnecting when idle");
    tcpClient.stop();
    resetPendingTransmission();
    return;
  }

  queueOldestResult(chipId, deviceName);

  flushOneNonBlockingWrite(currentMillis);
}
