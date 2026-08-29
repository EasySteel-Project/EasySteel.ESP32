#pragma once

#include <Arduino.h>
#include <WiFiClient.h>

constexpr int TCP_PROTOCOL_VERSION = 1;
constexpr size_t TCP_EVENT_MAX_BYTES = 384;
constexpr size_t TCP_JSON_MAX_BYTES = TCP_EVENT_MAX_BYTES - 2;
constexpr size_t TCP_RUN_ID_BYTES = 33;
constexpr size_t TCP_CONTEXT_ID_BYTES = 48;
constexpr size_t TCP_STAGE_NAME_BYTES = 32;
constexpr unsigned long TCP_MAX_ROUND_TIME_MS = 30000;
constexpr int TCP_MAX_TARGET_COUNT = 5;
constexpr int TCP_MAX_PENALTY_COUNT = 10;

enum class TcpServerMessageType : uint8_t
{
  Invalid,
  Unknown,
  Capabilities,
  Acknowledgement,
  MatchInfo,
  Ping
};

struct TcpServerMessage
{
  TcpServerMessageType type;
  char runId[TCP_RUN_ID_BYTES];
  char contextId[TCP_CONTEXT_ID_BYTES];
  char stage[TCP_STAGE_NAME_BYTES];
  uint32_t sequence;
};

/// Serialize one newline-terminated protocol event into a fixed caller-owned buffer.
/// Returns the byte count, or zero when the buffer is too small.
size_t tcpProtocolSerializeEvent(
  char *buffer,
  size_t capacity,
  const String &chipId,
  const String &deviceName,
  const char *event,
  int target,
  unsigned long timeMs,
  int targetCount,
  const char *runId = nullptr,
  unsigned long rawTimeMs = 0,
  int missCount = 0,
  int penaltyCount = 0,
  const char *contextId = nullptr,
  uint32_t sequence = 0);

/// Parse and validate one server-to-device JSON message.
/// Unknown v1 events are returned as TcpServerMessageType::Unknown.
bool tcpProtocolParseServerMessage(
  const char *json,
  size_t length,
  TcpServerMessage& message);

/// Compatibility helper used by host-side protocol tests.
void tcpProtocolWriteEvent(
  WiFiClient &client,
  const String &chipId,
  const String &deviceName,
  const char *event,
  int target,
  unsigned long timeMs,
  int targetCount,
  const char *runId = nullptr,
  unsigned long rawTimeMs = 0,
  int missCount = 0,
  int penaltyCount = 0,
  const char *contextId = nullptr,
  uint32_t sequence = 0);
