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

constexpr size_t OFFLINE_BUFFER_CAPACITY = 20;

/// Restore durable results at boot, even when Wi-Fi is unavailable. Fails closed.
bool offlineBufferInit();

/// Commit a physically confirmed run outside active rounds. Never evicts old results.
/// False means the caller must retain the pending result and show a failure.
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

/// Mark the oldest sent run acknowledged in RAM without performing any flash writes.
bool offlineBufferAcknowledge(const char *runId);

/// Persist acknowledged removal only when storageAllowed is true (no active round).
void offlineBufferTick(bool storageAllowed);

/// New official rounds require a healthy store and a free result slot.
bool offlineBufferCanAccept();

/// A storage error is latched until boot/reinitialization; never auto-clear the store.
bool offlineBufferHasStorageError();

/// Mark retained entries for retransmission after reconnecting.
void offlineBufferMarkAllUnsent();

/// Returns the number of buffered runs currently in memory.
size_t offlineBufferGetCount();
