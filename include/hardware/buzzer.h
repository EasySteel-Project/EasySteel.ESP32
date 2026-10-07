#pragma once

#include <Arduino.h>

enum class BuzzerPattern {
  None = 0,
  Start,       // Round start only: long beep (1000ms ON)
  Confirm,     // Confirmation: two short beeps (50ms ON, 50ms OFF, 50ms ON)
  DoubleShort, // Double short beep (50ms ON, 50ms OFF, 50ms ON)
  SingleShort, // Single short beep (30ms ON)
  MenuClick,   // Crisp hardware click (~2ms)
  StartupMelody // Startup melody (rhythmic beeps)
};

/// Extensible struct for custom/complex buzzer patterns (alternating ON/OFF durations)
struct CustomBuzzerPattern {
  const uint16_t *steps; // Array of durations in milliseconds
  uint8_t stepCount;     // Number of steps in the pattern
};

/// Initialize buzzer hardware pin.
void buzzerInit();

/// Play a predefined buzzer pattern.
void buzzerPlay(BuzzerPattern pattern, unsigned long currentMillis);

/// Play a custom buzzer pattern.
void buzzerPlayCustom(const CustomBuzzerPattern &pattern, unsigned long currentMillis);

/// Periodic update of the buzzer state. Call in main loop.
void buzzerTick(unsigned long currentMillis);
