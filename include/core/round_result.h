#pragma once

struct RoundResult
{
  unsigned long timeMs;
  int penaltyCount;
};

/// Store a finished result until the range officer confirms or discards it.
void roundResultPrepare(unsigned long timeMs, int penaltyCount);

/// Update the pending result after penalty changes.
void roundResultUpdate(unsigned long timeMs, int penaltyCount);

/// Consume the pending result exactly once.
bool roundResultTake(RoundResult &result);

/// Discard any pending result without submitting it.
void roundResultDiscard();

/// Return whether a result is waiting for confirmation.
bool roundResultIsPending();
