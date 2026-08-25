#include "leds.h"
#ifndef UNIT_TEST
#include "driver/gpio.h"
#endif

static unsigned long lastRedBlink = 0;
static bool redBlinkState = false;

void ledsInit()
{
  pinMode(LED_RED_PIN, OUTPUT);
  digitalWrite(LED_RED_PIN, LOW);

#ifndef UNIT_TEST
  // Set the red status LED (GPIO 4) drive capability to maximum (40mA).
  gpio_set_drive_capability((gpio_num_t)LED_RED_PIN, GPIO_DRIVE_CAP_3);
#endif

  for (int i = 0; i < MAX_TARGETS; i++)
  {
    pinMode(LED_GRN_PINS[i], OUTPUT);
    digitalWrite(LED_GRN_PINS[i], LOW);
  }
}

void ledRedSet(bool on)
{
  digitalWrite(LED_RED_PIN, on ? HIGH : LOW);
}

void ledRedBlink(unsigned long currentMillis)
{
  if (currentMillis - lastRedBlink >= RED_BLINK_INTERVAL_MS)
  {
    lastRedBlink = currentMillis;
    redBlinkState = !redBlinkState;
    digitalWrite(LED_RED_PIN, redBlinkState ? HIGH : LOW);
  }
}

void ledTargetSet(int index, bool on)
{
  if (index < 0 || index >= MAX_TARGETS)
    return;
  digitalWrite(LED_GRN_PINS[index], on ? HIGH : LOW);
}

void ledsReset()
{
  digitalWrite(LED_RED_PIN, LOW);
  redBlinkState = false;
  lastRedBlink = 0;

  for (int i = 0; i < MAX_TARGETS; i++)
  {
    digitalWrite(LED_GRN_PINS[i], LOW);
  }
}
