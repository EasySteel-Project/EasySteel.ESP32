#include "offline_buffer.h"
#include <Arduino.h>
#include <cstring>

enum class DeliveryState : uint8_t { Unsent, Queued, Sent };

struct BufferedRun
{
  char runId[24];
  unsigned long timeMs;
  int missedTargets;
  DeliveryState state;
};

static constexpr size_t BACKUP_BUFFER_SIZE = 20;
static BufferedRun backupBuffer[BACKUP_BUFFER_SIZE];
static size_t backupHead = 0;
static size_t backupTail = 0;
static size_t backupCount = 0;

bool offlineBufferPush(const char *runId, unsigned long timeMs, int missedTargets)
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
  run.timeMs = timeMs;
  run.missedTargets = missedTargets;
  run.state = DeliveryState::Unsent;
  backupHead = (backupHead + 1) % BACKUP_BUFFER_SIZE;
  backupCount++;

  Serial.print("[BACKUP] Buffered offline run: ");
  Serial.print(timeMs);
  Serial.print(" ms, missed/penalties: ");
  Serial.println(missedTargets);
  return retainedAllEntries;
}

bool offlineBufferPeekOldestUnsent(BufferedRunData& run)
{
  if (backupCount == 0 || backupBuffer[backupTail].state != DeliveryState::Unsent)
    return false;

  const BufferedRun& oldest = backupBuffer[backupTail];
  snprintf(run.runId, sizeof(run.runId), "%s", oldest.runId);
  run.timeMs = oldest.timeMs;
  run.missedTargets = oldest.missedTargets;
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

bool offlineBufferCompleteOldestSend(const char *runId, bool requireAck)
{
  if (backupCount == 0 || !runId)
    return false;

  BufferedRun& oldest = backupBuffer[backupTail];
  if (oldest.state != DeliveryState::Queued || strcmp(oldest.runId, runId) != 0)
    return false;

  if (requireAck)
  {
    oldest.state = DeliveryState::Sent;
    return true;
  }

  backupTail = (backupTail + 1) % BACKUP_BUFFER_SIZE;
  backupCount--;
  Serial.println("[OUTBOX] Sent buffered run to legacy server");
  return true;
}

bool offlineBufferAcknowledge(const char *runId)
{
  if (backupCount == 0 || !runId)
    return false;

  const BufferedRun& oldest = backupBuffer[backupTail];
  if (oldest.state != DeliveryState::Sent || strcmp(oldest.runId, runId) != 0)
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
