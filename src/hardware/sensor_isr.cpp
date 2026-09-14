#include "sensor_isr.h"

// State flags shared between ISR and main loop (marked volatile to prevent compiler caching)
static volatile bool isRunningState = false;
static volatile bool isTestModeState = false;
static volatile bool targetHit[MAX_TARGETS] = {};
static volatile unsigned long lastTrigger[MAX_TARGETS] = {};
static volatile unsigned long lastAnyHitTime = 0;

static constexpr uint8_t HIT_QUEUE_CAPACITY = 16;
static volatile HitEvent hitQueue[HIT_QUEUE_CAPACITY] = {};
static volatile uint8_t hitQueueHead = 0;
static volatile uint8_t hitQueueTail = 0;
static volatile uint8_t hitQueueCount = 0;
static volatile unsigned int droppedHitCount = 0;

#ifndef UNIT_TEST
#include <freertos/FreeRTOS.h>
static portMUX_TYPE hitQueueMux = portMUX_INITIALIZER_UNLOCKED;
#define HIT_QUEUE_ISR_LOCK() portENTER_CRITICAL_ISR(&hitQueueMux)
#define HIT_QUEUE_ISR_UNLOCK() portEXIT_CRITICAL_ISR(&hitQueueMux)
#define HIT_QUEUE_LOCK() portENTER_CRITICAL(&hitQueueMux)
#define HIT_QUEUE_UNLOCK() portEXIT_CRITICAL(&hitQueueMux)
#else
#define HIT_QUEUE_ISR_LOCK()
#define HIT_QUEUE_ISR_UNLOCK()
#define HIT_QUEUE_LOCK()
#define HIT_QUEUE_UNLOCK()
#endif

// Unified sensor hit handler
#ifdef UNIT_TEST
void onSensorHitIdx(int i)
#else
// IRAM_ATTR forces the compiler to store this ISR in Instruction RAM for sub-millisecond latency
static void IRAM_ATTR onSensorHitIdx(int i)
#endif
{
  if (i < 0 || i >= MAX_TARGETS)
    return;

  // Ignore hits if the round is not running
  if (!isRunningState)
    return;

  unsigned long now = millis();
  HIT_QUEUE_ISR_LOCK();

  // Recheck all shared state while holding the queue lock because reset may run concurrently.
  if (!isRunningState || (!isTestModeState && i != 0 && targetHit[i]) ||
      now - lastTrigger[i] <= SENSOR_DEBOUNCE_DELAY_MS ||
      now - lastAnyHitTime <= GLOBAL_CROSSTALK_LIMIT_MS)
  {
    HIT_QUEUE_ISR_UNLOCK();
    return;
  }

  if (hitQueueCount >= HIT_QUEUE_CAPACITY)
  {
    droppedHitCount++;
    HIT_QUEUE_ISR_UNLOCK();
    return;
  }

  lastTrigger[i] = now;
  lastAnyHitTime = now;
  targetHit[i] = true;
  hitQueue[hitQueueHead].targetIndex = i;
  hitQueue[hitQueueHead].time = now;
  hitQueueHead = (hitQueueHead + 1) % HIT_QUEUE_CAPACITY;
  hitQueueCount++;
  HIT_QUEUE_ISR_UNLOCK();
}

// Individual static ISR stubs for attachInterrupt (since C-style function pointers are required)
static void IRAM_ATTR onSensorHit0() { onSensorHitIdx(0); }
static void IRAM_ATTR onSensorHit1() { onSensorHitIdx(1); }
static void IRAM_ATTR onSensorHit2() { onSensorHitIdx(2); }
static void IRAM_ATTR onSensorHit3() { onSensorHitIdx(3); }
static void IRAM_ATTR onSensorHit4() { onSensorHitIdx(4); }

static void (*isrTable[MAX_TARGETS])() = {
  onSensorHit0, onSensorHit1, onSensorHit2, onSensorHit3, onSensorHit4
};

// Enable hardware target interrupts
void sensorIsrEnable(bool isRunning)
{
  isRunningState = isRunning;
  for (int i = 0; i < MAX_TARGETS; i++)
  {
#ifndef UNIT_TEST
    pinMode(SENSOR_PINS[i], INPUT);
#endif
    attachInterrupt(digitalPinToInterrupt(SENSOR_PINS[i]), isrTable[i], RISING);
  }
}

// Disable hardware target interrupts
void sensorIsrDisable()
{
  isRunningState = false;
  isTestModeState = false;
  for (int i = 0; i < MAX_TARGETS; i++)
  {
    detachInterrupt(digitalPinToInterrupt(SENSOR_PINS[i]));
  }
}

// Dynamically toggles whether target hit interrupts are processed
void sensorIsrSetRunning(bool running)
{
  isRunningState = running;
}

// Enable/disable test mode where all targets can be hit repeatedly
void sensorIsrSetTestMode(bool isTest)
{
  isTestModeState = isTest;
}

// Reset target status variables at start of round
void sensorIsrReset()
{
  HIT_QUEUE_LOCK();
  for (int i = 0; i < MAX_TARGETS; i++)
  {
    targetHit[i] = false;
    lastTrigger[i] = 0;
  }
  hitQueueHead = 0;
  hitQueueTail = 0;
  hitQueueCount = 0;
  droppedHitCount = 0;
  HIT_QUEUE_UNLOCK();
}

// Checks if a target has been hit during this round
bool sensorIsrWasHit(int targetIndex)
{
  if (targetIndex < 0 || targetIndex >= MAX_TARGETS)
    return false;
  return targetHit[targetIndex];
}

// Thread-safe fetch of the oldest pending hit event for the main loop
bool sensorIsrPopNextHit(int &targetIndex, unsigned long &hitTime)
{
  bool hasHit = false;
  HIT_QUEUE_LOCK();
  if (hitQueueCount > 0)
  {
    targetIndex = hitQueue[hitQueueTail].targetIndex;
    hitTime = hitQueue[hitQueueTail].time;
    hitQueueTail = (hitQueueTail + 1) % HIT_QUEUE_CAPACITY;
    hitQueueCount--;
    hasHit = true;
  }
  HIT_QUEUE_UNLOCK();
  return hasHit;
}

unsigned int sensorIsrTakeDroppedHitCount()
{
  HIT_QUEUE_LOCK();
  unsigned int count = droppedHitCount;
  droppedHitCount = 0;
  HIT_QUEUE_UNLOCK();
  return count;
}

unsigned long sensorIsrGetLastAnyHitTime()
{
  return lastAnyHitTime;
}

void sensorIsrResetLastAnyHitTime()
{
  HIT_QUEUE_LOCK();
  lastAnyHitTime = 0;
  HIT_QUEUE_UNLOCK();
}
