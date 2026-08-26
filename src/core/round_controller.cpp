#include "round_controller.h"
#include "buzzer.h"
#include "button.h"
#include "display.h"
#include "leds.h"
#include "menu.h"
#include "network.h"
#include "storage.h"
#include "strings.h"
#include "sensor_isr.h"
#include "round_result.h"
#include "scoring_rules.h"
#include "round_state.h"

static Button* btnStart = nullptr;
static Button* btnStop = nullptr;
static Button* btnReset = nullptr;

// --- State Machine ---
static volatile State currentState = STANDBY;
static unsigned long delayEndTime = 0;
static unsigned long falseStartStartedAt = 0;

// --- Input cooldown (prevents contact-bounce double-clicks) ---
static unsigned long inputCooldownUntil = 0;
static constexpr unsigned long INPUT_COOLDOWN_MS = 300;

// --- Auto-Power-Off / Screensaver Tracking ---
static unsigned long lastActivityTime = 0;
static bool screensaverActive = false;
static int screensaverTimeoutMinutes = 5;

// --- Timing and Scoring variables ---
static unsigned long startTime = 0;                         // Round start timestamp (after delay beep)
static unsigned long endTime = 0;                           // Current displayed time (including penalties)
static unsigned long baseEndTime = 0;                       // Raw time of the stop plate hit (no penalties)
static int penaltyCount = 0;                                // Number of +3.0s penalties added manually (max 10)
static int automaticMissCount = 0;                          // Misses calculated when the round finishes
static unsigned long lastRunningDispUpdate = 0;             // Running time display update timer
static int displayCycleIndex = 0;                           // Currently shown split item index (0=overall, >0=split hit)
static unsigned long lastRoundTimeMs = 0;                   // Last completed round time in ms
static int lastRoundPenaltyCount = 0;                       // Last completed round penalty count
static char currentStageName[32] = "";                      // Current stage name
static char currentContextId[48] = "";                      // Opaque match context echoed with confirmed results

// --- Button press state-tracking for Long-Press detection ---

static bool startHoldActive = false;
static unsigned long startPressTime = 0;
static bool startLongHandled = false;

static bool stopHoldActive = false;
static unsigned long stopPressTime = 0;
static bool stopLongHandled = false;

static bool resetHoldActive = false;
static unsigned long resetPressTime = 0;
static bool resetLongHandled = false;

struct HitRecord
{
  int targetId;
  unsigned long splitTime;
};
#define MAX_RECORDED_HITS 16
static HitRecord recordedHits[MAX_RECORDED_HITS];
static int recordedHitCount = 0;

// --- Shared Helpers ---

static void renderStandbyScreen()
{
  displayUpdateStandby(
    networkGetDeviceName().c_str(),
    currentStageName,
    lastRoundTimeMs,
    lastRoundPenaltyCount,
    networkIsServerConnected(),
    menuGetTargetCount(),
    menuGetRoundMode()
  );
}


static void resetToStandby()
{
  roundResultDiscard();
  sensorIsrDisable();
  currentState = STANDBY;
  ledsReset();
  sensorIsrReset();
  recordedHitCount = 0;
  baseEndTime = 0;
  penaltyCount = 0;
  automaticMissCount = 0;
  displayCycleIndex = 0;
  startHoldActive = false;
  startLongHandled = false;
  stopHoldActive = false;
  stopLongHandled = false;
  resetHoldActive = false;
  resetLongHandled = false;

  btnStart->wasPressed();
  btnStart->wasReleased();
  btnStop->wasPressed();
  btnStop->wasReleased();
  btnReset->wasPressed();
  btnReset->wasReleased();
  if (screensaverActive)
  {
    screensaverActive = false;
    displayExitScreensaver();
  }

  renderStandbyScreen();
  Serial.println("[ROUND] Reset to STANDBY");
}

static unsigned long getLastStopPlateTime()
{
  for (int i = recordedHitCount - 1; i >= 0; i--)
  {
    if (recordedHits[i].targetId == 0)
    {
      return recordedHits[i].splitTime;
    }
  }
  return 0; // not hit
}

static bool isStopPlateHitLast()
{
  if (recordedHitCount == 0)
    return false;
  return recordedHits[recordedHitCount - 1].targetId == 0;
}

static bool allIntermediateTargetsHit()
{
  int targetCount = menuGetTargetCount();
  for (int i = 1; i < targetCount; i++)
  {
    if (!sensorIsrWasHit(i))
      return false;
  }
  return true;
}

static void updateFinishedDisplay()
{
  int roundMode = menuGetRoundMode();
  int targetCount = menuGetTargetCount();
  bool hitMask[MAX_TARGETS] = {false};
  for (int i = 0; i < MAX_TARGETS; i++)
  {
    hitMask[i] = sensorIsrWasHit(i);
  }

  char timeStr[16];
  unsigned long secs = endTime / 1000;
  unsigned long ms = endTime % 1000;
  snprintf(timeStr, sizeof(timeStr), "%lu.%03lu", secs, ms);

  if (roundMode == 2)
  {
    // Reaction Drill Mode result
    int hitTgt = (recordedHitCount > 0) ? recordedHits[0].targetId : 0;
    displayUpdateReaction(timeStr, hitTgt);
    return;
  }

  if (displayCycleIndex == 0)
  {
    // Overall result
    displayUpdateFinished(timeStr, penaltyCount, hitMask, targetCount, STR_FOOTER_FINISHED);
  }
  else if (displayCycleIndex <= recordedHitCount)
  {
    // Recorded hit (chronological order)
    HitRecord hr = recordedHits[displayCycleIndex - 1];
    char splitStr[16];
    unsigned long sSecs = hr.splitTime / 1000;
    unsigned long sMs = hr.splitTime % 1000;
    snprintf(splitStr, sizeof(splitStr), "%lu.%03lu", sSecs, sMs);

    char deltaStr[16] = "";
    unsigned long prevTime = (displayCycleIndex == 1) ? 0 : recordedHits[displayCycleIndex - 2].splitTime;
    unsigned long delta = hr.splitTime - prevTime;
    snprintf(deltaStr, sizeof(deltaStr), "%lu.%03lu", delta / 1000, delta % 1000);

    displayUpdateSplitDetail(displayCycleIndex, recordedHitCount, hr.targetId, splitStr, deltaStr, penaltyCount);
  }
}



static int calculateMissedTargets()
{
  bool hitMask[MAX_TARGETS] = {};
  for (int targetIndex = 0; targetIndex < MAX_TARGETS; targetIndex++)
    hitMask[targetIndex] = sensorIsrWasHit(targetIndex);

  return scoringRulesCalculateMissedTargets(menuGetTargetCount(), hitMask);
}

static void finishRound(unsigned long totalTimeMs, const char *reason, unsigned long currentMillis, int missedTargets)
{
  sensorIsrDisable();
  currentState = FINISHED;
  baseEndTime = totalTimeMs;
  penaltyCount = missedTargets;
  automaticMissCount = missedTargets;
  unsigned long calcTime = baseEndTime + (unsigned long)penaltyCount * 3000;
  if (calcTime > ROUND_TIMEOUT_MS) calcTime = ROUND_TIMEOUT_MS;
  endTime = calcTime;
  lastRoundTimeMs = endTime;
  lastRoundPenaltyCount = penaltyCount;
  roundResultPrepare(baseEndTime, endTime, automaticMissCount, penaltyCount);

  displayCycleIndex = 0;
  startHoldActive = false;
  startLongHandled = false;
  stopHoldActive = false;
  stopLongHandled = false;
  resetHoldActive = false;
  resetLongHandled = false;
  ledRedSet(false);
  buzzerPlay(BuzzerPattern::DoubleShort, currentMillis);

  btnStart->wasPressed();
  btnStart->wasReleased();
  btnStop->wasPressed();
  btnStop->wasReleased();
  btnReset->wasPressed();
  btnReset->wasReleased();

  updateFinishedDisplay();

  Serial.print("[ROUND] Round ended (");
  Serial.print(reason);
  Serial.print("): ");
  Serial.print(totalTimeMs);
  Serial.println(" ms");
}

static void startRound(unsigned long currentMillis)
{
  currentState = RUNNING;
  startTime = currentMillis;
  ledRedSet(true);
  sensorIsrReset();
  recordedHitCount = 0;
  baseEndTime = 0;
  penaltyCount = 0;
  automaticMissCount = 0;
  sensorIsrResetLastAnyHitTime();

  for (int i = 0; i < MAX_TARGETS; i++)
  {
    ledTargetSet(i, false);
  }

  sensorIsrEnable(true);
  buzzerPlay(BuzzerPattern::Start, currentMillis);
  lastRunningDispUpdate = currentMillis;
  displayUpdateBigTime(STR_GO, "0.0", "s");
  networkSendEvent("start", -1, 0);
  Serial.println("[ROUND] Round started!");
}

// --- Hit Processing ---

static void processHits(unsigned long currentMillis)
{
  bool stopPlateHitThisPass = false;
  int i = -1;
  unsigned long hitTime = 0;

  while (sensorIsrPopNextHit(i, hitTime))
  {
    if (!scoringRulesShouldRecordTarget(i, menuGetTargetCount(), menuGetRoundMode()))
      continue;

    ledTargetSet(i, true);

    unsigned long splitTime = hitTime - startTime;
    networkSendEvent("hit", i, splitTime);

    if (recordedHitCount < MAX_RECORDED_HITS)
    {
      recordedHits[recordedHitCount] = { i, splitTime };
      recordedHitCount++;
    }

    if (i == 0)
    {
      stopPlateHitThisPass = true;
      buzzerPlay(BuzzerPattern::DoubleShort, hitTime);
      Serial.print("[HIT] STOP PLATE! Split Time: ");
    }
    else
    {
      if (!stopPlateHitThisPass)
      {
        buzzerPlay(BuzzerPattern::SingleShort, hitTime);
      }
      Serial.print("[HIT] Target ");
      Serial.print(i + 1);
      Serial.print("! Split Time: ");
    }
    Serial.print(splitTime);
    Serial.println(" ms");
  }
}

static void submitFinishedResult()
{
  RoundResult result = {};
  if (roundResultTake(result))
  {
    networkSendResult(
      result.rawTimeMs,
      result.timeMs,
      result.missCount,
      result.penaltyCount,
      currentContextId);
  }
}

// --- State Input Handlers ---

static void handleStandbyInput(unsigned long currentMillis, bool startPressed)
{
  static unsigned long menuEntryTime = 0;
  if (btnStop->isDown() && btnReset->isDown() && !btnStart->isDown())
  {
    if (menuEntryTime == 0)
    {
      menuEntryTime = currentMillis;
    }
    else if (currentMillis - menuEntryTime >= MENU_HOLD_MS)
    {
      menuEntryTime = 0;
      currentState = MENU;
      menuEnter(currentMillis);
      inputCooldownUntil = currentMillis + INPUT_COOLDOWN_MS;
    }
    return;
  }
  else
  {
    menuEntryTime = 0;
  }

  if (startPressed)
  {
    networkSendEvent("start_pressed", -1, 0);
    unsigned long delayMs = esp_random() % (START_DELAY_MAX_MS - START_DELAY_MIN_MS) + START_DELAY_MIN_MS;
    delayEndTime = currentMillis + delayMs;
    Serial.print("[BTN] Start pressed. Delaying for ");
    Serial.print(delayMs);
    Serial.println(" ms...");

    ledRedSet(true);
    sensorIsrReset();
    sensorIsrEnable(true);
    sensorIsrSetRunning(true);
    currentState = DELAY;
    displayUpdate(STR_PREPARE, STR_WAIT_FOR_BEEP);
  }
}

// --- Public API ---


void roundControllerInit(HwButtons& buttons)
{
  btnStart = &buttons.start;
  btnStop = &buttons.stop;
  btnReset = &buttons.reset;
  menuInit(buttons);

  buzzerInit();
  ledsInit();

  int targetCount = storageLoadTargetCount();
  menuSetTargetCount(targetCount);

  bool requireAll = storageLoadRequireAllTargets();
  menuSetRequireAllTargets(requireAll);

  screensaverTimeoutMinutes = storageLoadScreensaverTimeout();

  for (int i = 0; i < MAX_TARGETS; i++)
  {
    pinMode(SENSOR_PINS[i], INPUT_PULLDOWN);
  }

  lastActivityTime = millis();
}

int roundControllerGetScreensaverTimeout()
{
  return screensaverTimeoutMinutes;
}

void roundControllerSetScreensaverTimeout(int minutes)
{
  screensaverTimeoutMinutes = minutes;
}

int roundControllerGetTargetCount()
{
  return menuGetTargetCount();
}

bool roundControllerIsIdle()
{
  return currentState == STANDBY;
}

const char *roundControllerGetStageName()
{
  return currentStageName;
}

const char *roundControllerGetContextId()
{
  return currentContextId;
}

void roundControllerSetMatchInfo(const char *contextId, const char *stage)
{
  if (contextId && stage)
  {
    snprintf(currentContextId, sizeof(currentContextId), "%s", contextId);
    snprintf(currentStageName, sizeof(currentStageName), "%s", stage);
  }
  else
  {
    currentContextId[0] = '\0';
    currentStageName[0] = '\0';
  }
  if (currentState == STANDBY)
  {
    renderStandbyScreen();
  }
}

unsigned long roundControllerGetLastRoundTimeMs()
{
  return lastRoundTimeMs;
}

int roundControllerGetLastPenaltyCount()
{
  return lastRoundPenaltyCount;
}

void roundControllerTick(unsigned long currentMillis)
{
  buzzerTick(currentMillis);

  if (currentState == MENU)
  {
    if (!menuTick(currentMillis))
    {
      resetToStandby();
    }
    return;
  }

  if (currentMillis < inputCooldownUntil)
    return;

  bool startPressed = btnStart->wasPressed();
  bool stopPressed = btnStop->wasPressed();
  bool resetPressed = btnReset->wasPressed();

  if (startPressed || stopPressed || resetPressed)
  {
    lastActivityTime = currentMillis;
    if (screensaverActive)
    {
      screensaverActive = false;
      displayExitScreensaver();
      renderStandbyScreen();
      inputCooldownUntil = currentMillis + INPUT_COOLDOWN_MS;
      return;
    }
  }

  if (resetPressed && roundStateAllowsImmediateReset(currentState))
  {
    resetToStandby();
    networkSendEvent("reset", -1, 0);
    Serial.println("[BTN] Reset pressed during active round");
    inputCooldownUntil = currentMillis + INPUT_COOLDOWN_MS;
    return;
  }

  switch (currentState)
  {
  case STANDBY:
    if (screensaverTimeoutMinutes > 0 && currentMillis - lastActivityTime >= (unsigned long)screensaverTimeoutMinutes * 60000UL)
    {
      screensaverActive = true;
      displayScreensaverTick(currentMillis, networkGetDeviceName().c_str(), networkIsServerConnected());
      return;
    }

    handleStandbyInput(currentMillis, startPressed);

    {
      static unsigned long lastStandbyRefreshMs = 0;
      static bool lastServerState = false;
      bool currentServerState = networkIsServerConnected();
      if (currentServerState != lastServerState || currentMillis - lastStandbyRefreshMs >= 2000)
      {
        lastServerState = currentServerState;
        lastStandbyRefreshMs = currentMillis;
        renderStandbyScreen();
      }
    }
    break;

  case DELAY:
  {
    lastActivityTime = currentMillis;
    ledRedBlink(currentMillis);

    // Abort countdown on Stop button (Red) or Reset button (Black)
    if (stopPressed || resetPressed)
    {
      sensorIsrDisable();
      resetToStandby();
      buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
      networkSendEvent("reset", -1, 0);
      Serial.println("[DELAY] Countdown aborted by button press");
      inputCooldownUntil = currentMillis + INPUT_COOLDOWN_MS;
      return;
    }

    // False start detection: early sensor hit before beep
    int falseStartTarget = -1;
    unsigned long falseStartTime = 0;
    if (sensorIsrPopNextHit(falseStartTarget, falseStartTime))
    {
      sensorIsrDisable();
      buzzerPlay(BuzzerPattern::DoubleShort, currentMillis);
      displayUpdate(STR_FALSE_START, STR_FALSE_START_HINT);
      Serial.println("[ROUND] False start detected!");
      falseStartStartedAt = currentMillis;
      currentState = roundStateOnFalseStart(currentState);
      return;
    }

    if (currentMillis >= delayEndTime)
    {
      startRound(currentMillis);
    }
    break;
  }

  case FALSE_START:
    lastActivityTime = currentMillis;
    if (roundStateAdvanceFalseStart(currentState, falseStartStartedAt, currentMillis) == STANDBY)
    {
      resetToStandby();
      inputCooldownUntil = currentMillis + INPUT_COOLDOWN_MS;
    }
    break;

  case RUNNING:
    lastActivityTime = currentMillis;
    processHits(currentMillis);

    if (sensorIsrTakeDroppedHitCount() > 0)
    {
      Serial.println("[ROUND] Sensor queue overflow; discarding invalid round");
      networkSendEvent("reset", -1, 0);
      buzzerPlay(BuzzerPattern::DoubleShort, currentMillis);
      resetToStandby();
      return;
    }

    if (startPressed)
    {
      unsigned long lastStop = getLastStopPlateTime();
      unsigned long elapsed = currentMillis - startTime;
      unsigned long finalTime = (lastStop > 0) ? lastStop : elapsed;
      int missed = calculateMissedTargets();
      finishRound(finalTime, "Green Button", currentMillis, missed);
      inputCooldownUntil = currentMillis + INPUT_COOLDOWN_MS;
      return;
    }

    if (stopPressed)
    {
      unsigned long lastStop = getLastStopPlateTime();
      unsigned long elapsed = currentMillis - startTime;
      unsigned long finalTime = (lastStop > 0) ? lastStop : elapsed;
      int missed = calculateMissedTargets();
      finishRound(finalTime, "Red Button", currentMillis, missed);
      inputCooldownUntil = currentMillis + INPUT_COOLDOWN_MS;
      return;
    }

    if (menuGetRoundMode() == 2)
    {
      // Reaction Drill: First hit on any target ends the drill immediately
      for (int i = 0; i < MAX_TARGETS; i++)
      {
        if (sensorIsrWasHit(i))
        {
          unsigned long lastStop = getLastStopPlateTime();
          unsigned long elapsed = currentMillis - startTime;
          unsigned long finalTime = (recordedHitCount > 0) ? recordedHits[0].splitTime : ((lastStop > 0) ? lastStop : elapsed);
          finishRound(finalTime, "Reaction Hit", currentMillis, 0);
          return;
        }
      }
    }
    else
    {
      bool requireAll = menuGetRequireAllTargets();
      bool finishCondition = requireAll
        ? (allIntermediateTargetsHit() && isStopPlateHitLast())
        : sensorIsrWasHit(0);

      if (finishCondition)
      {
        unsigned long lastStop = getLastStopPlateTime();
        unsigned long elapsed = currentMillis - startTime;
        unsigned long finalTime = (lastStop > 0) ? lastStop : elapsed;
        int missed = calculateMissedTargets();
        finishRound(finalTime, "Stop Plate", currentMillis, missed);
        return;
      }
    }


    if (currentMillis - startTime >= ROUND_TIMEOUT_MS)
    {
      unsigned long lastStop = getLastStopPlateTime();
      unsigned long finalTime = (lastStop > 0) ? lastStop : ROUND_TIMEOUT_MS;
      int missed = calculateMissedTargets();
      finishRound(finalTime, "Timeout", currentMillis, missed);
      return;
    }

    if (currentMillis - lastRunningDispUpdate >= 100)
    {
      lastRunningDispUpdate = currentMillis;
      unsigned long elapsed = currentMillis - startTime;
      float progress = (float)elapsed / (float)ROUND_TIMEOUT_MS;

      unsigned long dispTimeMs = elapsed;
      if (!menuGetRequireAllTargets() && isStopPlateHitLast())
      {
        unsigned long lastStop = getLastStopPlateTime();
        if (lastStop > 0)
        {
          dispTimeMs = lastStop;
        }
      }

      unsigned long secs = dispTimeMs / 1000;
      unsigned long ms = (dispTimeMs % 1000) / 100;
      char runningTimeStr[16];
      snprintf(runningTimeStr, sizeof(runningTimeStr), "%lu.%lu", secs, ms);

      bool hitMask[MAX_TARGETS] = {false};
      for (int i = 0; i < MAX_TARGETS; i++)
      {
        hitMask[i] = sensorIsrWasHit(i);
      }
      displayUpdateRunning(runningTimeStr, progress, hitMask, menuGetTargetCount());
    }
    break;


  case FINISHED:
    if (resetPressed)
    {
      resetHoldActive = true;
      resetPressTime = currentMillis;
      resetLongHandled = false;
    }

    if (resetHoldActive && btnReset->isDown() && !resetLongHandled)
    {
      if (currentMillis - resetPressTime >= 1000)
      {
        resetLongHandled = true;
        resetHoldActive = false;
        resetToStandby();
        networkSendEvent("reset", -1, 0);
        buzzerPlay(BuzzerPattern::Start, currentMillis);
        Serial.println("[BTN] Reset button held (Long Press) - Round Reset");
        inputCooldownUntil = currentMillis + INPUT_COOLDOWN_MS;
        return;
      }
    }

    if (btnReset->wasReleased() && !resetLongHandled && resetHoldActive)
    {
      resetHoldActive = false;
      submitFinishedResult();
      buzzerPlay(BuzzerPattern::Confirm, currentMillis);
      Serial.print("[BTN] Reset button short press - Round Scored: ");
      Serial.print(endTime);
      Serial.print(" ms, Total Penalties/Missed: ");
      Serial.println(penaltyCount);
      resetToStandby();
      inputCooldownUntil = currentMillis + INPUT_COOLDOWN_MS;
      return;
    }

    if (startPressed)
    {
      if (menuGetRoundMode() == 2)
      {
        // Confirm the previous reaction result before immediately starting the next attempt.
        submitFinishedResult();
        resetToStandby();
        handleStandbyInput(currentMillis, true);
        return;
      }
      startHoldActive = true;
      startPressTime = currentMillis;
      startLongHandled = false;
    }


    if (startHoldActive && btnStart->isDown() && !startLongHandled)
    {
      if (currentMillis - startPressTime >= 1000)
      {
        startLongHandled = true;
        startHoldActive = false;
        displayCycleIndex = 0;
        buzzerPlay(BuzzerPattern::DoubleShort, currentMillis);
        updateFinishedDisplay();
        Serial.println("[BTN] Green button held (Long Press) - Reset view to overall end time");
      }
    }

    if (btnStart->wasReleased() && !startLongHandled && startHoldActive)
    {
      startHoldActive = false;
      int totalItems = recordedHitCount + 1;
      displayCycleIndex = (displayCycleIndex + 1) % totalItems;
      buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
      updateFinishedDisplay();
    }

    if (stopPressed)
    {
      stopHoldActive = true;
      stopPressTime = currentMillis;
      stopLongHandled = false;
    }

    if (stopHoldActive && btnStop->isDown() && !stopLongHandled)
    {
      if (currentMillis - stopPressTime >= 1000)
      {
        stopLongHandled = true;
        stopHoldActive = false;
        int minPenalty = menuGetRequireAllTargets() ? 0 : calculateMissedTargets();
        penaltyCount = minPenalty;
        unsigned long calcTime = baseEndTime + (unsigned long)penaltyCount * 3000;
        if (calcTime > ROUND_TIMEOUT_MS)
        {
          calcTime = ROUND_TIMEOUT_MS;
        }
        endTime = calcTime;
        lastRoundTimeMs = endTime;
        lastRoundPenaltyCount = penaltyCount;
        roundResultUpdate(baseEndTime, endTime, automaticMissCount, penaltyCount);
        displayCycleIndex = 0;
        buzzerPlay(BuzzerPattern::DoubleShort, currentMillis);
        updateFinishedDisplay();
        Serial.println("[BTN] Red button held (Long Press) - Reset penalties to min missed targets");
      }
    }

    if (btnStop->wasReleased() && !stopLongHandled && stopHoldActive)
    {
      stopHoldActive = false;
      int minPenalty = menuGetRequireAllTargets() ? 0 : calculateMissedTargets();
      penaltyCount++;
      if (penaltyCount > 10)
      {
        penaltyCount = minPenalty;
      }
      if (penaltyCount < minPenalty)
      {
        penaltyCount = minPenalty;
      }
      unsigned long calcTime = baseEndTime + (unsigned long)penaltyCount * 3000;
      if (calcTime > ROUND_TIMEOUT_MS)
      {
        calcTime = ROUND_TIMEOUT_MS;
      }
      endTime = calcTime;
      lastRoundTimeMs = endTime;
      lastRoundPenaltyCount = penaltyCount;
      roundResultUpdate(baseEndTime, endTime, automaticMissCount, penaltyCount);
      displayCycleIndex = 0;

      buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
      updateFinishedDisplay();
    }


    if (screensaverTimeoutMinutes > 0 && currentMillis - lastActivityTime >= (unsigned long)screensaverTimeoutMinutes * 60000UL)
    {
      resetToStandby();
    }
    break;

  case MENU:
    lastActivityTime = currentMillis;
    break;
  }
}
