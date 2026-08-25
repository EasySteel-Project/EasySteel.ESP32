#include "scoring_rules.h"

bool scoringRulesShouldRecordTarget(int targetIndex, int targetCount, int roundMode)
{
  if (targetIndex < 0 || targetIndex >= MAX_TARGETS)
    return false;

  // Reaction drill intentionally accepts the first hit from any connected sensor.
  if (roundMode == 2)
    return true;

  return targetIndex == 0 || targetIndex < targetCount;
}

int scoringRulesCalculateMissedTargets(int targetCount, const bool hitMask[MAX_TARGETS])
{
  if (targetCount < 1)
    targetCount = 1;
  if (targetCount > MAX_TARGETS)
    targetCount = MAX_TARGETS;

  int missedTargets = 0;
  for (int targetIndex = 1; targetIndex < targetCount; targetIndex++)
  {
    if (!hitMask[targetIndex])
      missedTargets++;
  }
  return missedTargets;
}
