#include "round_state.h"

State roundStateOnFalseStart(State currentState)
{
  return currentState == DELAY ? FALSE_START : currentState;
}

State roundStateAdvanceFalseStart(
  State currentState,
  uint32_t falseStartStartedAt,
  uint32_t currentMillis)
{
  if (currentState != FALSE_START)
    return currentState;

  if (currentMillis - falseStartStartedAt >= FALSE_START_DISPLAY_MS)
    return STANDBY;

  return FALSE_START;
}

bool roundStateAllowsImmediateReset(State currentState)
{
  return currentState == DELAY || currentState == RUNNING || currentState == FALSE_START;
}

uint32_t roundStateNextCountdownId(uint32_t currentId)
{
  if (currentId == UINT32_MAX)
  {
    return 1U;
  }
  return currentId + 1U;
}

uint32_t roundStateGetStartDelayRemainingMs(
  State currentState,
  uint32_t delayEndTime,
  uint32_t currentMillis)
{
  if (currentState != DELAY)
  {
    return 0U;
  }

  int32_t remainingMs = static_cast<int32_t>(delayEndTime - currentMillis);
  return remainingMs > 0 ? static_cast<uint32_t>(remainingMs) : 0U;
}
