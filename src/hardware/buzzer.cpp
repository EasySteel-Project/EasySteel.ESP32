#include "buzzer.h"
#include "config.h"

static unsigned long stepStartTime = 0;
static const uint16_t *currentSteps = nullptr;
static uint8_t currentStepCount = 0;
static uint8_t currentStepIndex = 0;

static const uint16_t START_STEPS[] = { 1000 };
static const uint16_t CONFIRM_STEPS[] = { 150 };
static const uint16_t DOUBLE_SHORT_STEPS[] = { 50, 50, 50 };
static const uint16_t SINGLE_SHORT_STEPS[] = { 30 };
static const uint16_t STARTUP_MELODY_STEPS[] = { 100, 50, 100, 50, 100, 150, 250, 150, 100, 50, 350 };

void buzzerInit()
{
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

void buzzerPlayCustom(const CustomBuzzerPattern &pattern, unsigned long currentMillis)
{
  currentSteps = pattern.steps;
  currentStepCount = pattern.stepCount;
  currentStepIndex = 0;
  stepStartTime = currentMillis;

  if (currentStepCount > 0)
  {
    digitalWrite(BUZZER_PIN, HIGH); // First step is ON
  }
  else
  {
    digitalWrite(BUZZER_PIN, LOW);
  }
}

void buzzerPlay(BuzzerPattern pattern, unsigned long currentMillis)
{
  // MenuClick is handled with a precise direct micro-delay
  // to avoid loop latency from stretching the click.
  if (pattern == BuzzerPattern::MenuClick)
  {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(2);
    digitalWrite(BUZZER_PIN, LOW);

    // Clear any active pattern
    currentSteps = nullptr;
    currentStepCount = 0;
    currentStepIndex = 0;
    return;
  }

  CustomBuzzerPattern custom;
  switch (pattern)
  {
    case BuzzerPattern::Start:
      custom.steps = START_STEPS;
      custom.stepCount = sizeof(START_STEPS) / sizeof(START_STEPS[0]);
      break;
    case BuzzerPattern::Confirm:
      custom.steps = CONFIRM_STEPS;
      custom.stepCount = sizeof(CONFIRM_STEPS) / sizeof(CONFIRM_STEPS[0]);
      break;
    case BuzzerPattern::DoubleShort:
      custom.steps = DOUBLE_SHORT_STEPS;
      custom.stepCount = sizeof(DOUBLE_SHORT_STEPS) / sizeof(DOUBLE_SHORT_STEPS[0]);
      break;
    case BuzzerPattern::SingleShort:
      custom.steps = SINGLE_SHORT_STEPS;
      custom.stepCount = sizeof(SINGLE_SHORT_STEPS) / sizeof(SINGLE_SHORT_STEPS[0]);
      break;
    case BuzzerPattern::StartupMelody:
      custom.steps = STARTUP_MELODY_STEPS;
      custom.stepCount = sizeof(STARTUP_MELODY_STEPS) / sizeof(STARTUP_MELODY_STEPS[0]);
      break;
    case BuzzerPattern::None:
    default:
      currentSteps = nullptr;
      currentStepCount = 0;
      currentStepIndex = 0;
      digitalWrite(BUZZER_PIN, LOW);
      return;
  }
  buzzerPlayCustom(custom, currentMillis);
}

void buzzerTick(unsigned long currentMillis)
{
  if (currentStepCount == 0 || currentSteps == nullptr)
  {
    return;
  }

  unsigned long elapsed = currentMillis - stepStartTime;
  if (elapsed >= currentSteps[currentStepIndex])
  {
    currentStepIndex++;
    stepStartTime = currentMillis;

    if (currentStepIndex >= currentStepCount)
    {
      currentSteps = nullptr;
      currentStepCount = 0;
      currentStepIndex = 0;
      digitalWrite(BUZZER_PIN, LOW);
    }
    else
    {
      // Even step index means buzzer is ON, odd step index means buzzer is OFF
      bool state = (currentStepIndex % 2 == 0);
      digitalWrite(BUZZER_PIN, state ? HIGH : LOW);
    }
  }
}
