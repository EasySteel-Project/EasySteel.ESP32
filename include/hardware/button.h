#pragma once

#include <Arduino.h>

class Button
{
public:
  /// Constructor. pin: GPIO pin, pullUp: true if using internal INPUT_PULLUP, activeLow: true if LOW means pressed.
  explicit Button(uint8_t pin, bool pullUp = true, bool activeLow = true);

  /// Initialize the button pin configuration.
  void init();

  /// Update button state. Call this in loop(). Returns true if state changed.
  bool update(unsigned long currentMillis);

  /// Returns true if the button transition from unpressed to pressed occurred since last check.
  bool wasPressed();

  /// Returns true if the button transition from pressed to unpressed occurred since last check.
  bool wasReleased();

  /// Consume the current press, including its hold and release events, until the next press.
  void consumeCurrentPress();

  /// Returns true if the button is currently held down.
  bool isDown() const;

  /// Returns true if the button is currently held down for at least durationMs.
  bool isHeldFor(unsigned long durationMs, unsigned long currentMillis) const;

  /// Returns the current click count in a rapid click window.
  /// Increments on press. Resets if clickWindowMs passes since last click.
  uint8_t getClickCount() const;

  /// Manually reset the click count.
  void resetClickCount();

private:
  uint8_t _pin;
  bool _pullUp;
  bool _activeLow;

  bool _state;         // Debounced state (true = pressed, false = released)
  bool _lastRawState;  // Last raw sensor value read
  unsigned long _lastDebounceTime;

  bool _wasPressed;
  bool _wasReleased;
  bool _pressConsumed;

  unsigned long _pressStartTime;

  uint8_t _clickCount;
  unsigned long _lastClickTime;

  static constexpr unsigned long DEBOUNCE_DELAY_MS = 30;
  static constexpr unsigned long CLICK_WINDOW_MS = 400; // Time window to count multi-clicks
};
