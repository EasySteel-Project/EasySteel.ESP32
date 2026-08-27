#pragma once

#include "config.h"
#include "hw_context.h"
#include "i18n.h"

/// Initialize menu with button references.
void menuInit(HwButtons& buttons);

/// Enter the menu. Call when menu entry conditions are met.
void menuEnter(unsigned long currentMillis);

/// Process menu input and display. Returns true while menu is active.
/// When it returns false, the menu has exited and round handling should resume.
bool menuTick(unsigned long currentMillis);

/// Get the configured target count after menu or round-controller initialization.
int menuGetTargetCount();

/// Set the target count (used during initialization from storage).
void menuSetTargetCount(int count);

/// Get whether all targets must be hit to stop the clock (false = Wettkampf/Strict, true = Alle Ziele/Training).
bool menuGetRequireAllTargets();

/// Set whether all targets must be hit to stop the clock.
void menuSetRequireAllTargets(bool requireAll);

/// Get the active round mode: 0=Wettkampf, 1=Alle Ziele, 2=Reaktions-Drill.
int menuGetRoundMode();

/// Set the active round mode.
void menuSetRoundMode(int mode);


/// Get the configured screensaver timeout in minutes.
int menuGetScreensaverTimeout();

/// Set the screensaver timeout in minutes.
void menuSetScreensaverTimeout(int minutes);

/// Get the configured language.
Language menuGetLanguage();

/// Set the language (used during initialization from storage).
void menuSetLanguage(Language lang);

