#pragma once

#include <Arduino.h>
#include <WiFiClient.h>

/// Initialize TCP client state.
void tcpClientInit();

/// Periodic tick to handle TCP connection retries and heartbeats.
void tcpClientTick(unsigned long currentMillis, bool provisioning, const String& serverIP, uint16_t tcpPort, const String& chipId, const String& deviceName);

/// Send a best-effort JSON event to the external timing server.
void tcpClientSendEvent(const String& chipId, const String& deviceName, bool provisioning, const char *event, int target, unsigned long timeMs);

/// Commit a physically confirmed result while FINISHED. False means not accepted.
bool tcpClientSendResult(
  bool provisioning,
  unsigned long rawTimeMs,
  unsigned long timeMs,
  int missCount,
  int penaltyCount,
  const char *contextId);

/// Returns true if currently connected to the TCP timing server.
bool tcpClientIsConnected();

/// Disconnect current TCP socket.
void tcpClientDisconnect();
