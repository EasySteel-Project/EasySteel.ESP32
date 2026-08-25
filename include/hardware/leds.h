#pragma once

#include "config.h"

/// Initialize LED hardware pins (red + green targets).
void ledsInit();

/// Set the red LED on or off.
void ledRedSet(bool on);

/// Fast-blink the red LED. Call every loop iteration during DELAY state.
void ledRedBlink(unsigned long currentMillis);

/// Set a specific green target LED on or off.
void ledTargetSet(int index, bool on);

/// Turn all LEDs off and reset blink state.
void ledsReset();
