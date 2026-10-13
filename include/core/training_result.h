#pragma once

#include "config.h"

struct TrainingHit
{
  int targetId;
  unsigned long splitTimeMs;
};

struct TrainingResult
{
  char id[24];
  unsigned long rawTimeMs;
  unsigned long timeMs;
  int mode;
  int targetCount;
  int missCount;
  int penaltyCount;
  bool hitMask[MAX_TARGETS];
  TrainingHit hits[MAX_RECORDED_HITS];
  int hitCount;
};

/// Reset the volatile latest solo-training result snapshot.
void trainingResultReset();

/// Publish one completed run for non-authoritative phone-local history.
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
  int hitCount);

/// Copy the latest completed run. Returns false until a run has finished.
bool trainingResultGet(TrainingResult &result);

/// Correct the latest run without changing its identity or chronological hits.
void trainingResultUpdateScore(unsigned long timeMs, int missCount, int penaltyCount);
