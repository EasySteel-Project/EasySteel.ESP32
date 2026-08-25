#pragma once

#include <Arduino.h>
#include <WiFiClient.h>

/// Initialize TCP client state.
void tcpClientInit();

/// Periodic tick to handle TCP connection retries and heartbeats.
void tcpClientTick(unsigned long currentMillis, bool provisioning, const String& serverIP, uint16_t tcpPort, const String& chipId, const String& deviceName);

/// Send a JSON event to the external timing server or retain a final result in the outbox.
void tcpClientSendEvent(const String& chipId, const String& deviceName, bool provisioning, const char *event, int target, unsigned long timeMs, int missedTargets);

/// Returns true if currently connected to the TCP timing server.
bool tcpClientIsConnected();

/// Disconnect current TCP socket.
void tcpClientDisconnect();
