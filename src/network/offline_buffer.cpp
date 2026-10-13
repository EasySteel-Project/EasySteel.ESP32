#include "offline_buffer.h"
#include "outbox_storage.h"
#include <Arduino.h>
#include <cstring>

enum class DeliveryState : uint8_t { Unsent, Queued, Sent, Acknowledged };

struct BufferedRun
{
  char runId[TCP_RUN_ID_BYTES];
  char contextId[TCP_CONTEXT_ID_BYTES];
  unsigned long rawTimeMs;
  unsigned long timeMs;
  int missCount;
  int penaltyCount;
  int targetCount;
  DeliveryState state;
  bool acknowledgementEligible;
  unsigned long sentAtMs;
};

static constexpr size_t BACKUP_BUFFER_SIZE = OFFLINE_BUFFER_CAPACITY;
static BufferedRun backupBuffer[BACKUP_BUFFER_SIZE];
static size_t backupHead = 0;
static size_t backupTail = 0;
static size_t backupCount = 0;
static bool storageReady = false;
static bool storageError = false;

// Fixed-width little-endian format, independent of struct padding or host ABI.
static constexpr size_t SNAPSHOT_HEADER_BYTES = 16;
static constexpr size_t SNAPSHOT_RUN_BYTES = TCP_RUN_ID_BYTES + TCP_CONTEXT_ID_BYTES + 20;
static constexpr size_t SNAPSHOT_BYTES = SNAPSHOT_HEADER_BYTES + BACKUP_BUFFER_SIZE * SNAPSHOT_RUN_BYTES;
static uint8_t snapshot[SNAPSHOT_BYTES];
static constexpr uint32_t SNAPSHOT_MAGIC = 0x424f5345; // ESOB
static constexpr uint32_t SNAPSHOT_VERSION = 1;

static void writeUint32(uint8_t *bytes, uint32_t value)
{
  for (size_t index = 0; index < 4; index++)
  {
    bytes[index] = static_cast<uint8_t>(value >> (index * 8));
  }
}

static uint32_t readUint32(const uint8_t *bytes)
{
  uint32_t value = 0;
  for (size_t index = 0; index < 4; index++)
  {
    value |= static_cast<uint32_t>(bytes[index]) << (index * 8);
  }
  return value;
}

static uint32_t snapshotChecksum()
{
  uint32_t crc = 0xffffffffU;
  for (size_t index = 0; index < SNAPSHOT_BYTES; index++)
  {
    // The checksum field itself is excluded.
    if (index >= 12 && index < SNAPSHOT_HEADER_BYTES)
    {
      continue;
    }
    crc ^= snapshot[index];
    for (int bit = 0; bit < 8; bit++)
    {
      crc = (crc >> 1) ^ ((crc & 1U) ? 0xedb88320U : 0U);
    }
  }
  return ~crc;
}

static void encodeRun(uint8_t *bytes, const BufferedRun& run)
{
  memcpy(bytes, run.runId, TCP_RUN_ID_BYTES);
  bytes += TCP_RUN_ID_BYTES;
  memcpy(bytes, run.contextId, TCP_CONTEXT_ID_BYTES);
  bytes += TCP_CONTEXT_ID_BYTES;
  writeUint32(bytes, run.rawTimeMs);
  writeUint32(bytes + 4, run.timeMs);
  writeUint32(bytes + 8, static_cast<uint32_t>(run.missCount));
  writeUint32(bytes + 12, static_cast<uint32_t>(run.penaltyCount));
  writeUint32(bytes + 16, static_cast<uint32_t>(run.targetCount));
}

static bool decodeRun(const uint8_t *bytes, BufferedRun& run)
{
  run = {};
  memcpy(run.runId, bytes, TCP_RUN_ID_BYTES);
  bytes += TCP_RUN_ID_BYTES;
  memcpy(run.contextId, bytes, TCP_CONTEXT_ID_BYTES);
  bytes += TCP_CONTEXT_ID_BYTES;
  run.rawTimeMs = readUint32(bytes);
  run.timeMs = readUint32(bytes + 4);
  const uint32_t misses = readUint32(bytes + 8);
  const uint32_t penalties = readUint32(bytes + 12);
  const uint32_t targets = readUint32(bytes + 16);
  if (run.runId[0] == '\0' || !memchr(run.runId, '\0', sizeof(run.runId)) ||
      !memchr(run.contextId, '\0', sizeof(run.contextId)) ||
      run.rawTimeMs > TCP_MAX_ROUND_TIME_MS || run.timeMs > TCP_MAX_ROUND_TIME_MS ||
      run.timeMs < run.rawTimeMs || targets < 1 || targets > TCP_MAX_TARGET_COUNT ||
      misses >= targets || penalties < misses || penalties > TCP_MAX_PENALTY_COUNT)
  {
    return false;
  }
  run.missCount = static_cast<int>(misses);
  run.penaltyCount = static_cast<int>(penalties);
  run.targetCount = static_cast<int>(targets);
  run.state = DeliveryState::Unsent;
  return true;
}

static bool persistSnapshot(const BufferedRun *appended = nullptr, size_t skipped = 0)
{
  memset(snapshot, 0, sizeof(snapshot));
  writeUint32(snapshot, SNAPSHOT_MAGIC);
  writeUint32(snapshot + 4, SNAPSHOT_VERSION);
  const size_t retained = backupCount - skipped;
  writeUint32(snapshot + 8, static_cast<uint32_t>(retained + (appended ? 1 : 0)));
  for (size_t index = 0; index < retained; index++)
  {
    encodeRun(snapshot + SNAPSHOT_HEADER_BYTES + index * SNAPSHOT_RUN_BYTES,
      backupBuffer[(backupTail + skipped + index) % BACKUP_BUFFER_SIZE]);
  }
  if (appended)
  {
    encodeRun(snapshot + SNAPSHOT_HEADER_BYTES + retained * SNAPSHOT_RUN_BYTES, *appended);
  }
  writeUint32(snapshot + 12, snapshotChecksum());
  if (!outboxStorageWrite(snapshot, sizeof(snapshot)))
  {
    storageError = true;
    Serial.println("[OUTBOX] Storage commit failed; official admission blocked until restart");
    return false;
  }
  return true;
}

bool offlineBufferInit()
{
  backupHead = 0;
  backupTail = 0;
  backupCount = 0;
  storageReady = false;
  storageError = false;
  size_t length = 0;
  const OutboxStorageReadResult loaded = outboxStorageRead(snapshot, sizeof(snapshot), length);
  if (loaded == OutboxStorageReadResult::Missing)
  {
    storageReady = persistSnapshot();
    return storageReady;
  }
  if (loaded != OutboxStorageReadResult::Loaded || length != sizeof(snapshot) ||
      readUint32(snapshot) != SNAPSHOT_MAGIC || readUint32(snapshot + 4) != SNAPSHOT_VERSION ||
      readUint32(snapshot + 8) > BACKUP_BUFFER_SIZE || readUint32(snapshot + 12) != snapshotChecksum())
  {
    storageError = true;
    return false;
  }
  const size_t count = readUint32(snapshot + 8);
  for (size_t index = 0; index < count; index++)
  {
    if (!decodeRun(snapshot + SNAPSHOT_HEADER_BYTES + index * SNAPSHOT_RUN_BYTES, backupBuffer[index]))
    {
      storageError = true;
      return false;
    }
  }
  backupCount = count;
  backupHead = count % BACKUP_BUFFER_SIZE;
  storageReady = true;
  return true;
}

bool offlineBufferPush(
  const char *runId,
  const char *contextId,
  unsigned long rawTimeMs,
  unsigned long timeMs,
  int missCount,
  int penaltyCount,
  int targetCount)
{
  if (!offlineBufferCanAccept() || !runId || !runId[0] || strlen(runId) >= TCP_RUN_ID_BYTES ||
      (contextId && strlen(contextId) >= TCP_CONTEXT_ID_BYTES) ||
      rawTimeMs > TCP_MAX_ROUND_TIME_MS || timeMs > TCP_MAX_ROUND_TIME_MS || timeMs < rawTimeMs ||
      targetCount < 1 || targetCount > TCP_MAX_TARGET_COUNT || missCount < 0 || missCount >= targetCount ||
      penaltyCount < missCount || penaltyCount > TCP_MAX_PENALTY_COUNT)
  {
    return false;
  }

  BufferedRun run = {};
  snprintf(run.runId, sizeof(run.runId), "%s", runId);
  snprintf(run.contextId, sizeof(run.contextId), "%s", contextId ? contextId : "");
  run.rawTimeMs = rawTimeMs;
  run.timeMs = timeMs;
  run.missCount = missCount;
  run.penaltyCount = penaltyCount;
  run.targetCount = targetCount;
  run.state = DeliveryState::Unsent;
  run.acknowledgementEligible = false;
  run.sentAtMs = 0;
  if (!persistSnapshot(&run))
  {
    return false;
  }
  backupBuffer[backupHead] = run;
  backupHead = (backupHead + 1) % BACKUP_BUFFER_SIZE;
  backupCount++;

  Serial.print("[BACKUP] Buffered offline run: ");
  Serial.print(timeMs);
  Serial.print(" ms, penalties: ");
  Serial.println(penaltyCount);
  return true;
}

bool offlineBufferPeekOldestUnsent(BufferedRunData& run)
{
  if (!storageReady || storageError || backupCount == 0 || backupBuffer[backupTail].state != DeliveryState::Unsent)
    return false;

  const BufferedRun& oldest = backupBuffer[backupTail];
  snprintf(run.runId, sizeof(run.runId), "%s", oldest.runId);
  snprintf(run.contextId, sizeof(run.contextId), "%s", oldest.contextId);
  run.rawTimeMs = oldest.rawTimeMs;
  run.timeMs = oldest.timeMs;
  run.missCount = oldest.missCount;
  run.penaltyCount = oldest.penaltyCount;
  run.targetCount = oldest.targetCount;
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

  BufferedRun& oldest = backupBuffer[backupTail];
  if (!oldest.acknowledgementEligible || strcmp(oldest.runId, runId) != 0)
    return false;

  oldest.state = DeliveryState::Acknowledged;
  return true;
}

void offlineBufferTick(bool storageAllowed)
{
  if (!storageAllowed || !storageReady || storageError || backupCount == 0 ||
      backupBuffer[backupTail].state != DeliveryState::Acknowledged)
  {
    return;
  }
  if (persistSnapshot(nullptr, 1))
  {
    backupTail = (backupTail + 1) % BACKUP_BUFFER_SIZE;
    backupCount--;
  }
}

bool offlineBufferCanAccept()
{
  return storageReady && !storageError && backupCount < BACKUP_BUFFER_SIZE;
}

bool offlineBufferHasStorageError()
{
  return !storageReady || storageError;
}

void offlineBufferMarkAllUnsent()
{
  for (size_t index = 0; index < backupCount; index++)
  {
    const size_t bufferIndex = (backupTail + index) % BACKUP_BUFFER_SIZE;
    if (backupBuffer[bufferIndex].state != DeliveryState::Acknowledged)
    {
      backupBuffer[bufferIndex].state = DeliveryState::Unsent;
    }
  }
}

size_t offlineBufferGetCount()
{
  return backupCount;
}

bool offlineBufferClear()
{
  backupHead = 0;
  backupTail = 0;
  backupCount = 0;
  storageError = false;
  memset(backupBuffer, 0, sizeof(backupBuffer));
  return persistSnapshot();
}
