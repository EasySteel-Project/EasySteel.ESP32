#include "round_result.h"

static RoundResult pendingResult = {};
static bool resultPending = false;

void roundResultPrepare(
  unsigned long rawTimeMs,
  unsigned long timeMs,
  int missCount,
  int penaltyCount)
{
  pendingResult = {rawTimeMs, timeMs, missCount, penaltyCount};
  resultPending = true;
}

void roundResultUpdate(
  unsigned long rawTimeMs,
  unsigned long timeMs,
  int missCount,
  int penaltyCount)
{
  if (!resultPending)
  {
    return;
  }

  pendingResult = {rawTimeMs, timeMs, missCount, penaltyCount};
}

bool roundResultTake(RoundResult &result)
{
  if (!resultPending)
  {
    return false;
  }

  result = pendingResult;
  resultPending = false;
  return true;
}

void roundResultDiscard()
{
  resultPending = false;
}

bool roundResultConfirm(bool (*submit)(const RoundResult&))
{
  if (!resultPending || !submit || !submit(pendingResult))
  {
    return false;
  }
  resultPending = false;
  return true;
}

bool roundResultIsPending()
{
  return resultPending;
}
