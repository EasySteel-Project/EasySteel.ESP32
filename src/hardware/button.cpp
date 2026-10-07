#include "button.h"

Button::Button(uint8_t pin, bool pullUp, bool activeLow)
  : _pin(pin), _pullUp(pullUp), _activeLow(activeLow),
    _state(false), _lastRawState(activeLow), _lastDebounceTime(0),
    _wasPressed(false), _wasReleased(false), _pressConsumed(false), _pressStartTime(0),
    _clickCount(0), _lastClickTime(0)
{
}

// Configures the GPIO pin mode and reads the initial state
void Button::init()
{
  pinMode(_pin, _pullUp ? INPUT_PULLUP : INPUT);
  bool rawVal = digitalRead(_pin);
  _lastRawState = rawVal;
  _state = _activeLow ? (rawVal == LOW) : (rawVal == HIGH);
}

// Non-blocking debounce and click tracking. Needs to be polled continuously in loop()
bool Button::update(unsigned long currentMillis)
{
  bool rawVal = digitalRead(_pin);
  bool stateChanged = false;

  // Reset debounce timer on any state transition
  if (rawVal != _lastRawState)
  {
    _lastDebounceTime = currentMillis;
  }

  // Filter out noise by waiting for the state to stabilize
  if ((currentMillis - _lastDebounceTime) > DEBOUNCE_DELAY_MS)
  {
    bool debouncedState = _activeLow ? (rawVal == LOW) : (rawVal == HIGH);
    if (debouncedState != _state)
    {
      _state = debouncedState;
      stateChanged = true;

      if (_state)
      {
        _pressConsumed = false;
        _wasPressed = true;
        _pressStartTime = currentMillis;

        // Detect consecutive fast clicks within the CLICK_WINDOW_MS
        if (currentMillis - _lastClickTime < CLICK_WINDOW_MS)
        {
          _clickCount++;
        }
        else
        {
          _clickCount = 1;
        }
        _lastClickTime = currentMillis;
      }
      else
      {
        _wasReleased = !_pressConsumed;
      }
    }
  }

  // Automatically reset click count if the click window expires without further press
  if (_clickCount > 0 && (currentMillis - _lastClickTime >= CLICK_WINDOW_MS))
  {
    _clickCount = 0;
  }

  _lastRawState = rawVal;
  return stateChanged;
}

// Returns true if the button was pressed since the last check, and resets the flag
bool Button::wasPressed()
{
  bool ret = _wasPressed;
  _wasPressed = false;
  return ret;
}

// Returns true if the button was released since the last check, and resets the flag
bool Button::wasReleased()
{
  bool ret = _wasReleased;
  _wasReleased = false;
  return ret;
}

void Button::consumeCurrentPress()
{
  _wasPressed = false;
  _wasReleased = false;
  _pressConsumed = true;
}

// Checks if the button is currently held down
bool Button::isDown() const
{
  return _state;
}

// Checks if the button has been held down continuously for a specified duration
bool Button::isHeldFor(unsigned long durationMs, unsigned long currentMillis) const
{
  return _state && !_pressConsumed && (currentMillis - _pressStartTime >= durationMs);
}

uint8_t Button::getClickCount() const
{
  return _clickCount;
}

void Button::resetClickCount()
{
  _clickCount = 0;
}
