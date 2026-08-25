#include "watchdog.h"
#include <esp_task_wdt.h>
#include <Arduino.h>

static bool initialized = false;

void watchdogInit(uint32_t timeoutSeconds)
{
  esp_task_wdt_init(timeoutSeconds, true);
  esp_task_wdt_add(NULL);
  initialized = true;
}

void watchdogFeed()
{
  if (initialized)
  {
    esp_task_wdt_reset();
  }
}

void watchdogDisable()
{
  if (initialized)
  {
    esp_task_wdt_delete(NULL);
  }
}

void watchdogEnable()
{
  if (initialized)
  {
    esp_task_wdt_add(NULL);
    esp_task_wdt_reset();
  }
}
