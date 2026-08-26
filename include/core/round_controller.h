#pragma once

#include "config.h"
#include "hw_context.h"

/// Initialize round hardware and controller state.
void roundControllerInit(HwButtons& buttons);

/// Advance the round controller. Call every loop iteration.
void roundControllerTick(unsigned long currentMillis);

/// Get the configured target count.
int roundControllerGetTargetCount();

/// Get the screensaver timeout in minutes (0 = disabled).
int roundControllerGetScreensaverTimeout();

/// Set the screensaver timeout in minutes.
void roundControllerSetScreensaverTimeout(int minutes);

/// Return true while the device is safely idle for configuration or firmware maintenance.
bool roundControllerIsIdle();

/// Get the current stage name (or empty string if none).
const char *roundControllerGetStageName();

/// Get the current opaque match context identifier (or empty string if none).
const char *roundControllerGetContextId();

/// Set or clear display-only match context while the controller is idle.
void roundControllerSetMatchInfo(const char *contextId, const char *stage);

/// Get the last completed round time in milliseconds.
unsigned long roundControllerGetLastRoundTimeMs();

/// Get the last completed round penalty count.
int roundControllerGetLastPenaltyCount();
