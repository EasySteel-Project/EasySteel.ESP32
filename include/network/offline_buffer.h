#pragma once

#include <stddef.h>

struct BufferedRunData
{
  char runId[24];
  unsigned long timeMs;
  int missedTargets;
};

/// Push a run to the RAM outbox. Returns false if the oldest entry had to be replaced.
bool offlineBufferPush(const char *runId, unsigned long timeMs, int missedTargets);

/// Copy the oldest run when it is ready to be queued for transmission.
bool offlineBufferPeekOldestUnsent(BufferedRunData& run);

/// Mark the oldest run as queued for socket transmission.
bool offlineBufferMarkOldestQueued(const char *runId);

/// Complete an actual socket send. ACK mode retains the run until acknowledgement.
bool offlineBufferCompleteOldestSend(const char *runId, bool requireAck);

/// Remove the oldest sent run if its ID matches the server acknowledgement.
bool offlineBufferAcknowledge(const char *runId);

/// Mark retained entries for retransmission after reconnecting.
void offlineBufferMarkAllUnsent();

/// Returns the number of buffered runs currently in memory.
size_t offlineBufferGetCount();

/// Clear all buffered runs in memory.
void offlineBufferClear();
