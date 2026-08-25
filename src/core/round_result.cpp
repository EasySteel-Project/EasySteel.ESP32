#include "round_result.h"

static RoundResult pendingResult = {};
static bool resultPending = false;

void roundResultPrepare(unsigned long timeMs, int penaltyCount)
{
  pendingResult = {timeMs, penaltyCount};
  resultPending = true;
}

void roundResultUpdate(unsigned long timeMs, int penaltyCount)
{
  if (!resultPending)
    return;

  pendingResult = {timeMs, penaltyCount};
}

bool roundResultTake(RoundResult &result)
{
  if (!resultPending)
    return false;

  result = pendingResult;
  resultPending = false;
  return true;
}

void roundResultDiscard()
{
  resultPending = false;
}

bool roundResultIsPending()
{
  return resultPending;
}
