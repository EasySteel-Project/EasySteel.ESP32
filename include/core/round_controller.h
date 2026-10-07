#pragma once

#include "config.h"
#include "hw_context.h"
#include "round_state.h"
#include "training_result.h"

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

/// Get current round state.
State roundControllerGetState();

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

/// Get the active round mode (0=Competition, 1=All Targets, 2=Reaction Drill).
int roundControllerGetRoundMode();

/// Set active round mode while idle. Returns true on success.
bool roundControllerSetRoundMode(int mode);

/// Get the current elapsed or final time in milliseconds.
unsigned long roundControllerGetElapsedMs(unsigned long currentMillis);

/// Get the stable identifier of the current or most recent start countdown.
unsigned long roundControllerGetCountdownId();

/// Get milliseconds remaining until the authoritative ESP32 start signal.
unsigned long roundControllerGetStartDelayRemainingMs(unsigned long currentMillis);

/// Get the configured preparation delay in seconds (0 = disabled, just random timer).
int roundControllerGetPreparationDelay();

/// Set the configured preparation delay in seconds (0..30). Returns true on success.
bool roundControllerSetPreparationDelay(int seconds);

/// Get milliseconds remaining of the preparation delay before the random window begins.
unsigned long roundControllerGetPrepDelayRemainingMs(unsigned long currentMillis);

/// Get current penalty count in FINISHED state.
int roundControllerGetPenaltyCount();

/// Get calculated automatic misses.
int roundControllerGetAutomaticMissCount();

/// Copy live hit mask of targets.
void roundControllerGetHitMask(bool* outMask, int maxTargets);

/// Copy recorded chronological hits. Returns number of hits copied.
int roundControllerGetRecordedHits(int* outTargetIds, unsigned long* outSplitTimes, int maxHits);

/// Copy the latest completed non-authoritative solo-training run.
bool roundControllerGetLatestTrainingResult(TrainingResult &result);
