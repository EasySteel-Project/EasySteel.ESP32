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

int scoringRulesCalculateMissedTargets(int targetCount, const bool hitMask[MAX_TARGETS], int roundMode)
{
  if (roundMode == 2)
  {
    return 0;
  }

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

unsigned long scoringRulesFinalTime(unsigned long rawTimeMs, int penaltyCount)
{
  if (rawTimeMs >= ROUND_TIMEOUT_MS) { return ROUND_TIMEOUT_MS; }
  const unsigned long penalties = penaltyCount > 0 ? static_cast<unsigned long>(penaltyCount) : 0;
  const unsigned long remaining = ROUND_TIMEOUT_MS - rawTimeMs;
  return penalties > remaining / 3000UL ? ROUND_TIMEOUT_MS : rawTimeMs + penalties * 3000UL;
}
