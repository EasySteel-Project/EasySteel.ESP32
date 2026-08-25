#pragma once

#include <stdint.h>

enum State { STANDBY, DELAY, RUNNING, FINISHED, MENU, FALSE_START };

constexpr uint32_t FALSE_START_DISPLAY_MS = 1200;

/// Enter false-start display only from the pre-start delay.
State roundStateOnFalseStart(State currentState);

/// Advance the false-start display using rollover-safe elapsed-time arithmetic.
State roundStateAdvanceFalseStart(
  State currentState,
  uint32_t falseStartStartedAt,
  uint32_t currentMillis);

/// Return whether Reset must abort the current active state immediately.
bool roundStateAllowsImmediateReset(State currentState);
