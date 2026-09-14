#pragma once

#include "tcp_protocol.h"
#include <stddef.h>

struct BufferedRunData
{
  char runId[TCP_RUN_ID_BYTES];
  char contextId[TCP_CONTEXT_ID_BYTES];
  unsigned long rawTimeMs;
  unsigned long timeMs;
  int missCount;
  int penaltyCount;
  int targetCount;
};

/// Push a run to the RAM outbox. Returns false if the oldest entry had to be replaced.
bool offlineBufferPush(
  const char *runId,
  const char *contextId,
  unsigned long rawTimeMs,
  unsigned long timeMs,
  int missCount,
  int penaltyCount,
  int targetCount);

/// Copy the oldest run when it is ready to be queued for transmission.
bool offlineBufferPeekOldestUnsent(BufferedRunData& run);

/// Mark the oldest run as queued for socket transmission.
bool offlineBufferMarkOldestQueued(const char *runId);

/// Complete an actual socket send and retain the run until acknowledgement.
bool offlineBufferCompleteOldestSend(
  const char *runId,
  unsigned long currentMillis);

/// Release an unacknowledged oldest result for retransmission after the timeout.
bool offlineBufferReleaseTimedOutAck(
  unsigned long currentMillis,
  unsigned long timeoutMs);

/// Remove the oldest sent run if its ID matches the server acknowledgement.
bool offlineBufferAcknowledge(const char *runId);

/// Mark retained entries for retransmission after reconnecting.
void offlineBufferMarkAllUnsent();

/// Returns the number of buffered runs currently in memory.
size_t offlineBufferGetCount();

/// Clear all buffered runs in memory.
void offlineBufferClear();
