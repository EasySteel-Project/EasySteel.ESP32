#pragma once
#include <stdint.h>

/// Initializes the Task Watchdog Timer with a specified timeout (default 4 seconds).
void watchdogInit(uint32_t timeoutSeconds = 4);

/// Resets (feeds) the Task Watchdog Timer.
void watchdogFeed();

/// Temporarily disables (unsubscribes) the current task from TWDT.
/// Useful during long blocking operations like OTA updates or boot animations.
void watchdogDisable();

/// Re-enables (subscribes) the current task to TWDT.
void watchdogEnable();
