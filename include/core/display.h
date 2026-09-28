#pragma once

#include "config.h"

/// Initialize display hardware. Returns true if display was detected.
bool displayInit();

/// Update display with two lines of text. No-op if display is absent.
void displayUpdate(const char *line1, const char *line2);

/// Update display with a big time value in the center and optional progress bar (0.0 to 1.0) or penalty dots (0 to 10).
void displayUpdateBigTime(const char *label, const char *numStr, const char *suffix, float progress = -1.0f, int penaltyCount = -1);

/// Update display for Standby mode showing lane/stage name, ready status, last result time, and active match config.
void displayUpdateStandby(const char *deviceName, const char *stageName, unsigned long lastTimeMs, int penaltyCount, bool serverConnected, int targetCount = 5, int roundMode = 0, const char *outboxStatus = nullptr);

/// Update display for active RUNNING state showing time, progress, and live hit target boxes.
void displayUpdateRunning(const char *timeStr, float progress, const bool hitMask[5], int targetCount);

/// Update display for FINISHED round showing overall time, penalty dots, target hit summary boxes, and footer legend.
void displayUpdateFinished(const char *timeStr, int penaltyCount, const bool hitMask[5], int targetCount, const char *footer = nullptr);

/// Update display for split detail inspection during FINISHED state.
void displayUpdateSplitDetail(int hitIndex, int totalHits, int targetId, const char *splitTimeStr, const char *deltaStr, int penaltyCount);

/// Update display for REACTION DRILL result showing draw time and prompt.
void displayUpdateReaction(const char *timeStr, int hitTarget);


/// Display startup animation on boot.
void displayShowStartupAnimation();

/// Update display with a scrollable 3-row menu list, highlighting the selected item.
void displayUpdateMenuList(const char *headerTitle, const char *const *items, int itemCount, int selectedIndex, const char *footer = nullptr);

/// Update display for editing a parameter value.
void displayUpdateValueEdit(const char *categoryTitle, const char *paramName, const char *valueStr, const char *hint = nullptr, const char *footer = nullptr);

/// Update display with a confirmation dialog.
void displayUpdateConfirmation(const char *title, const char *line1, const char *line2, const char *footer = nullptr);

/// Update display with a live 5-target diagnostic matrix for sensor hits.
void displayUpdateSensorMatrix(const bool hitTargets[5], int lastHitIndex);

/// Update display showing system and device diagnostic information.
void displayUpdateDeviceInfo(const char *fwVersion, const char *deviceName, const char *ip, const char *mac, unsigned long uptimeSec);

/// Render animated bouncing screensaver in Standby mode to prevent OLED burn-in.
void displayScreensaverTick(unsigned long currentMillis, const char *deviceName, bool isConnected);

/// Restore display from screensaver mode.
void displayExitScreensaver();
