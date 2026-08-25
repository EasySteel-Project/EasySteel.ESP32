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
