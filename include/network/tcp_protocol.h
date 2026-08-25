#pragma once

#include <Arduino.h>
#include <WiFiClient.h>

constexpr int TCP_PROTOCOL_VERSION = 1;
constexpr size_t TCP_EVENT_MAX_BYTES = 384;

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
  int missedTargets,
  const char *runId = nullptr);

/// Compatibility helper used by host-side protocol tests.
void tcpProtocolWriteEvent(
  WiFiClient &client,
  const String &chipId,
  const String &deviceName,
  const char *event,
  int target,
  unsigned long timeMs,
  int targetCount,
  int missedTargets,
  const char *runId = nullptr);
