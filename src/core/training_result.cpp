#include "training_result.h"

#include <stdio.h>
#include <string.h>

static TrainingResult latestResult = {};
static unsigned long resultRevision = 0;
static bool resultAvailable = false;

void trainingResultReset()
{
  memset(&latestResult, 0, sizeof(latestResult));
  resultRevision = 0;
  resultAvailable = false;
}

void trainingResultPublish(
  unsigned long nonce,
  unsigned long rawTimeMs,
  unsigned long timeMs,
  int mode,
  int targetCount,
  int missCount,
  int penaltyCount,
  const bool *hitMask,
  const int *targetIds,
  const unsigned long *splitTimes,
  int hitCount)
{
  memset(&latestResult, 0, sizeof(latestResult));
  resultRevision++;
  snprintf(latestResult.id, sizeof(latestResult.id), "%08lx-%08lx", nonce, resultRevision);
  latestResult.rawTimeMs = rawTimeMs;
  latestResult.timeMs = timeMs;
  latestResult.mode = mode;
  latestResult.targetCount = targetCount;
  latestResult.missCount = missCount;
  latestResult.penaltyCount = penaltyCount;

  for (int targetIndex = 0; targetIndex < MAX_TARGETS; targetIndex++)
  {
    latestResult.hitMask[targetIndex] = hitMask != nullptr && hitMask[targetIndex];
  }

  int boundedHitCount = hitCount;
  if (boundedHitCount < 0)
  {
    boundedHitCount = 0;
  }
  if (boundedHitCount > MAX_RECORDED_HITS)
  {
    boundedHitCount = MAX_RECORDED_HITS;
  }
  if (targetIds == nullptr || splitTimes == nullptr)
  {
    boundedHitCount = 0;
  }

  latestResult.hitCount = boundedHitCount;
  for (int hitIndex = 0; hitIndex < boundedHitCount; hitIndex++)
  {
    latestResult.hits[hitIndex] = {targetIds[hitIndex], splitTimes[hitIndex]};
  }

  resultAvailable = true;
}

bool trainingResultGet(TrainingResult &result)
{
  if (!resultAvailable)
  {
    return false;
  }

  result = latestResult;
  return true;
}
