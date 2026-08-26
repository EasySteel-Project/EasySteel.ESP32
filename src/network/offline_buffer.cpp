#include "offline_buffer.h"
#include <Arduino.h>
#include <cstring>

enum class DeliveryState : uint8_t { Unsent, Queued, Sent };

struct BufferedRun
{
  char runId[TCP_RUN_ID_BYTES];
  char contextId[TCP_CONTEXT_ID_BYTES];
  unsigned long rawTimeMs;
  unsigned long timeMs;
  int missCount;
  int penaltyCount;
  DeliveryState state;
  bool acknowledgementEligible;
  unsigned long sentAtMs;
};

static constexpr size_t BACKUP_BUFFER_SIZE = 20;
static BufferedRun backupBuffer[BACKUP_BUFFER_SIZE];
static size_t backupHead = 0;
static size_t backupTail = 0;
static size_t backupCount = 0;

bool offlineBufferPush(
  const char *runId,
  const char *contextId,
  unsigned long rawTimeMs,
  unsigned long timeMs,
  int missCount,
  int penaltyCount)
{
  bool retainedAllEntries = true;
  if (backupCount >= BACKUP_BUFFER_SIZE)
  {
    backupTail = (backupTail + 1) % BACKUP_BUFFER_SIZE;
    backupCount--;
    retainedAllEntries = false;
  }

  BufferedRun& run = backupBuffer[backupHead];
  snprintf(run.runId, sizeof(run.runId), "%s", runId ? runId : "");
  snprintf(run.contextId, sizeof(run.contextId), "%s", contextId ? contextId : "");
  run.rawTimeMs = rawTimeMs;
  run.timeMs = timeMs;
  run.missCount = missCount;
  run.penaltyCount = penaltyCount;
  run.state = DeliveryState::Unsent;
  run.acknowledgementEligible = false;
  run.sentAtMs = 0;
  backupHead = (backupHead + 1) % BACKUP_BUFFER_SIZE;
  backupCount++;

  Serial.print("[BACKUP] Buffered offline run: ");
  Serial.print(timeMs);
  Serial.print(" ms, penalties: ");
  Serial.println(penaltyCount);
  return retainedAllEntries;
}

bool offlineBufferPeekOldestUnsent(BufferedRunData& run)
{
  if (backupCount == 0 || backupBuffer[backupTail].state != DeliveryState::Unsent)
    return false;

  const BufferedRun& oldest = backupBuffer[backupTail];
  snprintf(run.runId, sizeof(run.runId), "%s", oldest.runId);
  snprintf(run.contextId, sizeof(run.contextId), "%s", oldest.contextId);
  run.rawTimeMs = oldest.rawTimeMs;
  run.timeMs = oldest.timeMs;
  run.missCount = oldest.missCount;
  run.penaltyCount = oldest.penaltyCount;
  return true;
}

bool offlineBufferMarkOldestQueued(const char *runId)
{
  if (backupCount == 0 || !runId)
    return false;

  BufferedRun& oldest = backupBuffer[backupTail];
  if (oldest.state != DeliveryState::Unsent || strcmp(oldest.runId, runId) != 0)
    return false;

  oldest.state = DeliveryState::Queued;
  return true;
}

bool offlineBufferCompleteOldestSend(
  const char *runId,
  unsigned long currentMillis)
{
  if (backupCount == 0 || !runId)
    return false;

  BufferedRun& oldest = backupBuffer[backupTail];
  if (oldest.state != DeliveryState::Queued || strcmp(oldest.runId, runId) != 0)
    return false;

  oldest.state = DeliveryState::Sent;
  oldest.acknowledgementEligible = true;
  oldest.sentAtMs = currentMillis;
  return true;
}

bool offlineBufferReleaseTimedOutAck(
  unsigned long currentMillis,
  unsigned long timeoutMs)
{
  if (backupCount == 0)
  {
    return false;
  }

  BufferedRun& oldest = backupBuffer[backupTail];
  if (oldest.state != DeliveryState::Sent ||
      currentMillis - oldest.sentAtMs < timeoutMs)
  {
    return false;
  }

  oldest.state = DeliveryState::Unsent;
  return true;
}

bool offlineBufferAcknowledge(const char *runId)
{
  if (backupCount == 0 || !runId)
    return false;

  const BufferedRun& oldest = backupBuffer[backupTail];
  if (!oldest.acknowledgementEligible || strcmp(oldest.runId, runId) != 0)
    return false;

  backupTail = (backupTail + 1) % BACKUP_BUFFER_SIZE;
  backupCount--;
  return true;
}

void offlineBufferMarkAllUnsent()
{
  for (size_t index = 0; index < backupCount; index++)
  {
    const size_t bufferIndex = (backupTail + index) % BACKUP_BUFFER_SIZE;
    backupBuffer[bufferIndex].state = DeliveryState::Unsent;
  }
}

size_t offlineBufferGetCount()
{
  return backupCount;
}

void offlineBufferClear()
{
  backupHead = 0;
  backupTail = 0;
  backupCount = 0;
}
