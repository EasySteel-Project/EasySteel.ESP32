#pragma once

#include "config.h"
#include <Arduino.h>

struct HitEvent
{
  int targetIndex;
  unsigned long time;
};

/// Enable GPIO sensor interrupts for steel targets.
void sensorIsrEnable(bool isRunning);

/// Disable GPIO sensor interrupts.
void sensorIsrDisable();

/// Reset sensor hit state and clear hit queue.
void sensorIsrReset();

/// Returns true if target with index i was hit in current run.
bool sensorIsrWasHit(int targetIndex);

/// Set sensor running state (used by ISR to ignore hits outside RUNNING state).
void sensorIsrSetRunning(bool running);

/// Enable/disable test mode where all targets can be hit repeatedly.
void sensorIsrSetTestMode(bool isTest);

/// Retrieve and consume the oldest hit event in chronological order.
bool sensorIsrPopNextHit(int &targetIndex, unsigned long &hitTime);

/// Return the number of events dropped because the ISR queue was full and reset the counter.
unsigned int sensorIsrTakeDroppedHitCount();

/// Get last global hit timestamp.
unsigned long sensorIsrGetLastAnyHitTime();

/// Reset last global hit timestamp.
void sensorIsrResetLastAnyHitTime();
