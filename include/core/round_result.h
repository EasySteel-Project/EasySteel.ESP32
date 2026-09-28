#pragma once

struct RoundResult
{
  unsigned long rawTimeMs;
  unsigned long timeMs;
  int missCount;
  int penaltyCount;
};

/// Store a finished result until the range officer confirms or discards it.
void roundResultPrepare(
  unsigned long rawTimeMs,
  unsigned long timeMs,
  int missCount,
  int penaltyCount);

/// Update the pending result after penalty changes.
void roundResultUpdate(
  unsigned long rawTimeMs,
  unsigned long timeMs,
  int missCount,
  int penaltyCount);

/// Consume the pending result exactly once.
bool roundResultTake(RoundResult &result);

/// Consume only after the submission callback reports durable acceptance.
/// The caller invokes this exclusively after a physical confirmation action.
bool roundResultConfirm(bool (*submit)(const RoundResult&));

/// Discard any pending result without submitting it.
void roundResultDiscard();

/// Return whether a result is waiting for confirmation.
bool roundResultIsPending();
