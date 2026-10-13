#include "round_controller.h"
#include "buzzer.h"
#include "button.h"
#include "display.h"
#include "leds.h"
#include "menu.h"
#include "network.h"
#include "storage.h"
#include "ui_strings.h"
#include "sensor_isr.h"
#include "round_result.h"
#include "scoring_rules.h"
#include "round_state.h"
#include "remote_control.h"
#include "training_result.h"
#include "offline_buffer.h"
#include <cstring>

static Button* btnStart = nullptr;
static Button* btnStop = nullptr;
static Button* btnReset = nullptr;

// --- State Machine ---
static volatile State currentState = STANDBY;
static unsigned long delayEndTime = 0;
static unsigned long countdownId = 0;
static unsigned long falseStartStartedAt = 0;
static unsigned long randomDelayStartTime = 0;
static unsigned long lastPrepDispUpdate = 0;

// --- Input cooldown (prevents contact-bounce double-clicks) ---
static unsigned long inputCooldownUntil = 0;
static constexpr unsigned long INPUT_COOLDOWN_MS = 300;

// --- Auto-Power-Off / Screensaver Tracking ---
static unsigned long lastActivityTime = 0;
static bool screensaverActive = false;

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
static bool currentRoundIsSoloTraining = false;             // Captured when a round starts with a paired phone and no PC
static bool confirmationFailed = false;
static char currentStageName[32] = "";                      // Current stage name
static char currentContextId[48] = "";                      // Opaque match context echoed with confirmed results
static bool matchSessionActive = false;                     // True when RangeControl is connected or match session is in progress

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
static HitRecord recordedHits[MAX_RECORDED_HITS];
static int recordedHitCount = 0;
static bool acceptedHits[MAX_TARGETS] = {};

// --- Shared Helpers ---

static bool isMatchRound()
{
  if (currentRoundIsSoloTraining)
  {
    return false;
  }
  return networkIsServerConnected() || (currentContextId[0] != '\0') || matchSessionActive;
}

bool roundControllerIsMatchActive()
{
  return isMatchRound();
}

void roundControllerClearMatchSession()
{
  matchSessionActive = false;
  currentContextId[0] = '\0';
  currentStageName[0] = '\0';
}

static void renderStandbyScreen()
{
  const bool matchActive = isMatchRound();
  if (matchActive && !offlineBufferCanAccept())
  {
    char pending[24];
    snprintf(pending, sizeof(pending), STR_OUTBOX_PENDING_FORMAT,
      static_cast<unsigned int>(offlineBufferGetCount()),
      static_cast<unsigned int>(OFFLINE_BUFFER_CAPACITY));
    const bool failed = offlineBufferHasStorageError();
    displayUpdateConfirmation(
      failed ? STR_OUTBOX_STORAGE_ERROR : STR_OUTBOX_FULL,
      failed ? STR_OUTBOX_SERVICE : pending,
      STR_OUTBOX_BLOCKED,
      failed ? STR_OUTBOX_SERVICE : STR_OUTBOX_CONNECT);
    return;
  }
  char pending[24] = {};
  if (matchActive && offlineBufferGetCount() > 0)
  {
    snprintf(pending, sizeof(pending), STR_OUTBOX_PENDING_FORMAT,
      static_cast<unsigned int>(offlineBufferGetCount()),
      static_cast<unsigned int>(OFFLINE_BUFFER_CAPACITY));
  }
  displayUpdateStandby(
    networkGetDeviceName().c_str(),
    currentStageName,
    lastRoundTimeMs,
    lastRoundPenaltyCount,
    networkIsServerConnected(),
    menuGetTargetCount(),
    menuGetRoundMode(),
    pending[0] ? pending : nullptr
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
  memset(acceptedHits, 0, sizeof(acceptedHits));
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
  currentRoundIsSoloTraining = false;
  confirmationFailed = false;
  randomDelayStartTime = 0;
  lastPrepDispUpdate = 0;

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
    if (!acceptedHits[i])
      return false;
  }
  return true;
}

static void renderConfirmationFailure()
{
  char timeStr[24];
  snprintf(timeStr, sizeof(timeStr), STR_TIME_FORMAT, endTime / 1000, endTime % 1000);
  displayUpdateConfirmation(
    offlineBufferHasStorageError() ? STR_OUTBOX_STORAGE_ERROR : STR_OUTBOX_FULL,
    STR_OUTBOX_KEEP_RESULT,
    timeStr,
    STR_OUTBOX_KEEP_POWER);
}

static void updateFinishedDisplay()
{
  if (confirmationFailed)
  {
    renderConfirmationFailure();
    return;
  }
  int roundMode = menuGetRoundMode();
  int targetCount = menuGetTargetCount();
  bool hitMask[MAX_TARGETS] = {false};
  for (int i = 0; i < MAX_TARGETS; i++)
  {
    hitMask[i] = acceptedHits[i];
  }

  char timeStr[16];
  unsigned long secs = endTime / 1000;
  unsigned long ms = endTime % 1000;
  snprintf(timeStr, sizeof(timeStr), STR_TIME_VALUE_FORMAT, secs, ms);

  if (roundMode == 2)
  {
    // Reaction Drill Mode result
    int hitTgt = (recordedHitCount > 0) ? recordedHits[0].targetId : -1;
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
    snprintf(splitStr, sizeof(splitStr), STR_TIME_VALUE_FORMAT, sSecs, sMs);

    char deltaStr[16] = "";
    unsigned long prevTime = (displayCycleIndex == 1) ? 0 : recordedHits[displayCycleIndex - 2].splitTime;
    unsigned long delta = hr.splitTime - prevTime;
    snprintf(deltaStr, sizeof(deltaStr), STR_TIME_VALUE_FORMAT, delta / 1000, delta % 1000);

    displayUpdateSplitDetail(displayCycleIndex, recordedHitCount, hr.targetId, splitStr, deltaStr, penaltyCount);
  }
}



static int calculateMissedTargets()
{
  if (menuGetRoundMode() == 2)
  {
    return 0;
  }

  bool hitMask[MAX_TARGETS] = {};
  for (int targetIndex = 0; targetIndex < MAX_TARGETS; targetIndex++)
    hitMask[targetIndex] = acceptedHits[targetIndex];

  return scoringRulesCalculateMissedTargets(menuGetTargetCount(), hitMask, menuGetRoundMode());
}

static void updateResultScore()
{
  endTime = scoringRulesFinalTime(baseEndTime, penaltyCount);
  lastRoundTimeMs = endTime;
  lastRoundPenaltyCount = penaltyCount;
  roundResultUpdate(baseEndTime, endTime, automaticMissCount, penaltyCount);
  if (currentRoundIsSoloTraining)
  {
    trainingResultUpdateScore(endTime, automaticMissCount, penaltyCount);
  }
}

static void finishRound(unsigned long totalTimeMs, const char *reason, unsigned long currentMillis, int missedTargets)
{
  sensorIsrDisable();
  currentState = FINISHED;
  baseEndTime = totalTimeMs > ROUND_TIMEOUT_MS ? ROUND_TIMEOUT_MS : totalTimeMs;
  penaltyCount = missedTargets;
  automaticMissCount = missedTargets;
  updateResultScore();
  roundResultPrepare(baseEndTime, endTime, automaticMissCount, penaltyCount);

  bool trainingHitMask[MAX_TARGETS] = {};
  int trainingTargetIds[MAX_RECORDED_HITS] = {};
  unsigned long trainingSplitTimes[MAX_RECORDED_HITS] = {};
  for (int targetIndex = 0; targetIndex < MAX_TARGETS; targetIndex++)
  {
    trainingHitMask[targetIndex] = acceptedHits[targetIndex];
  }
  for (int hitIndex = 0; hitIndex < recordedHitCount; hitIndex++)
  {
    trainingTargetIds[hitIndex] = recordedHits[hitIndex].targetId;
    trainingSplitTimes[hitIndex] = recordedHits[hitIndex].splitTime;
  }
  if (currentRoundIsSoloTraining)
  {
    trainingResultPublish(
      static_cast<unsigned long>(esp_random()),
      baseEndTime,
      endTime,
      menuGetRoundMode(),
      menuGetTargetCount(),
      automaticMissCount,
      penaltyCount,
      trainingHitMask,
      trainingTargetIds,
      trainingSplitTimes,
      recordedHitCount);
  }

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
  memset(acceptedHits, 0, sizeof(acceptedHits));
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
  displayUpdateBigTime(STR_GO, STR_INITIAL_TIME_VALUE, STR_UNIT_SECONDS);
  networkSendEvent("start", -1, 0);
  Serial.println("[ROUND] Round started!");
}

// --- Hit Processing ---

static void processHits()
{
  bool stopPlateHitThisPass = false;
  int i = -1;
  unsigned long hitTime = 0;

  while (sensorIsrPopNextHit(i, hitTime))
  {
    if (!scoringRulesShouldRecordTarget(i, menuGetTargetCount(), menuGetRoundMode()))
      continue;

    unsigned long splitTime = hitTime - startTime;
    if (splitTime > ROUND_TIMEOUT_MS)
    {
      break;
    }
    acceptedHits[i] = true;
    ledTargetSet(i, true);
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
    if (menuGetRoundMode() == 2 ||
        (i == 0 && (!menuGetRequireAllTargets() || allIntermediateTargetsHit())))
    {
      break;
    }
  }
}

static bool storeOfficialResult(const RoundResult& result)
{
  return networkSendResult(
    result.rawTimeMs,
    result.timeMs,
    result.missCount,
    result.penaltyCount,
    currentContextId);
}

static bool submitFinishedResult()
{
  if (!isMatchRound())
  {
    roundResultDiscard();
    return true;
  }

  if (!roundResultConfirm(storeOfficialResult))
  {
    confirmationFailed = true;
    renderConfirmationFailure();
    return false;
  }
  return true;
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
    currentRoundIsSoloTraining =
      remoteControlGetState() == RemoteControlState::Paired &&
      !remoteControlIsPcConnected();
    if (isMatchRound() && !offlineBufferCanAccept())
    {
      renderStandbyScreen();
      return;
    }
    networkSendEvent("start_pressed", -1, 0);
    unsigned long randomDelayMs = esp_random() % (START_DELAY_MAX_MS - START_DELAY_MIN_MS) + START_DELAY_MIN_MS;
    unsigned long prepDelayMs = static_cast<unsigned long>(menuGetPreparationDelay()) * 1000UL;
    countdownId = roundStateNextCountdownId(countdownId);
    if (menuGetPreparationDelay() > 0)
    {
      randomDelayStartTime = currentMillis + prepDelayMs;
      delayEndTime = randomDelayStartTime + randomDelayMs;
    }
    else
    {
      randomDelayStartTime = 0;
      delayEndTime = currentMillis + randomDelayMs;
    }
    lastPrepDispUpdate = currentMillis;
    Serial.print("[BTN] Start pressed. Total delay: ");
    Serial.print(prepDelayMs + randomDelayMs);
    Serial.print(" ms (Prep: ");
    Serial.print(prepDelayMs);
    Serial.print(" ms, Random: ");
    Serial.print(randomDelayMs);
    Serial.println(" ms)...");

    ledRedSet(true);
    sensorIsrReset();
    sensorIsrEnable(true);
    sensorIsrSetRunning(true);
    currentState = DELAY;
    if (menuGetPreparationDelay() > 0)
    {
      char prepStr[24];
      snprintf(prepStr, sizeof(prepStr), STR_PREP_COUNTDOWN_FORMAT, menuGetPreparationDelay());
      displayUpdate(STR_PREPARE, prepStr);
    }
    else
    {
      displayUpdate(STR_PREPARE, STR_WAIT_FOR_BEEP);
    }
  }
}

// --- Public API ---


void roundControllerInit(HwButtons& buttons)
{
  btnStart = &buttons.start;
  btnStop = &buttons.stop;
  btnReset = &buttons.reset;
  menuInit(buttons);
  trainingResultReset();

  buzzerInit();
  ledsInit();

  int targetCount = storageLoadTargetCount();
  menuSetTargetCount(targetCount);

  menuSetRoundMode(storageLoadRoundMode());
  menuSetScreensaverTimeout(storageLoadScreensaverTimeout());
  menuSetPreparationDelay(storageLoadPreparationDelay());

  int lang = storageLoadLanguage();
  menuSetLanguage(lang == 1 ? Language::EN : Language::DE);

  for (int i = 0; i < MAX_TARGETS; i++)
  {
    pinMode(SENSOR_PINS[i], INPUT);
  }

  lastActivityTime = millis();
  inputCooldownUntil = 0;
  lastRoundTimeMs = 0;
  lastRoundPenaltyCount = 0;
  roundControllerClearMatchSession();
  resetToStandby();
}

int roundControllerGetScreensaverTimeout()
{
  return menuGetScreensaverTimeout();
}

void roundControllerSetScreensaverTimeout(int minutes)
{
  menuSetScreensaverTimeout(minutes);
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
  if (contextId && stage && (contextId[0] != '\0' || stage[0] != '\0'))
  {
    snprintf(currentContextId, sizeof(currentContextId), "%s", contextId);
    snprintf(currentStageName, sizeof(currentStageName), "%s", stage);
    matchSessionActive = true;
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

  if (inputCooldownUntil != 0)
  {
    if (static_cast<long>(inputCooldownUntil - currentMillis) > 0)
    {
      return;
    }
    inputCooldownUntil = 0;
  }

  bool startPressed = btnStart->wasPressed();
  bool stopPressed = btnStop->wasPressed();
  bool resetPressed = btnReset->wasPressed();
  bool hardwareInputReceived = startPressed || stopPressed || resetPressed;
  bool remoteInputReceived = false;
  RemoteControlCommand remoteCommand = {RemoteControlAction::Start, 0};

  if (hardwareInputReceived)
  {
    remoteControlClearPendingCommand();
  }
  else
  {
    remoteInputReceived = remoteControlTakeCommand(remoteCommand, currentMillis, currentState, countdownId);
  }

  if (hardwareInputReceived || remoteInputReceived)
  {
    lastActivityTime = currentMillis;
    if (screensaverActive)
    {
      screensaverActive = false;
      displayExitScreensaver();
      renderStandbyScreen();
      if (hardwareInputReceived)
      {
        inputCooldownUntil = currentMillis + INPUT_COOLDOWN_MS;
        return;
      }
    }
  }

  if (remoteInputReceived)
  {
    if (remoteCommand.action == RemoteControlAction::SetMode)
    {
      roundControllerSetRoundMode(remoteCommand.value);
      inputCooldownUntil = currentMillis + INPUT_COOLDOWN_MS;
      return;
    }
    if (remoteCommand.action == RemoteControlAction::SetPrepDelay)
    {
      roundControllerSetPreparationDelay(remoteCommand.value);
      inputCooldownUntil = currentMillis + INPUT_COOLDOWN_MS;
      return;
    }
    if (remoteCommand.action == RemoteControlAction::Start)
    {
      if (currentState == STANDBY)
      {
        startPressed = true;
      }
      else if (currentState == FINISHED)
      {
        if (!currentRoundIsSoloTraining) { return; }
        resetToStandby();
        handleStandbyInput(currentMillis, true);
        inputCooldownUntil = currentMillis + INPUT_COOLDOWN_MS;
        return;
      }
      else
      {
        return;
      }
    }
    else if (remoteCommand.action == RemoteControlAction::Stop)
    {
      if (currentState != DELAY && currentState != RUNNING)
      {
        return;
      }
      stopPressed = true;
    }
    else if (remoteCommand.action == RemoteControlAction::Reset)
    {
      if (currentState == FINISHED)
      {
        if (!currentRoundIsSoloTraining) { return; }
        resetToStandby();
        networkSendEvent("reset", -1, 0);
        inputCooldownUntil = currentMillis + INPUT_COOLDOWN_MS;
        return;
      }
      if (currentState != DELAY && currentState != RUNNING)
      {
        return;
      }
      resetPressed = true;
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
    if ((!isMatchRound() || (offlineBufferGetCount() == 0 && !offlineBufferHasStorageError())) &&
        menuGetScreensaverTimeout() > 0 && currentMillis - lastActivityTime >= (unsigned long)menuGetScreensaverTimeout() * 60000UL)
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
      if (currentServerState)
      {
        matchSessionActive = true;
      }
      else if (offlineBufferGetCount() == 0 && currentContextId[0] == '\0')
      {
        matchSessionActive = false;
      }
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

    uint32_t prepRemainingMs = roundStateGetPrepDelayRemainingMs(currentState, randomDelayStartTime, currentMillis);
    if (prepRemainingMs > 0)
    {
      int discardedTarget = -1;
      unsigned long discardedTime = 0;
      while (sensorIsrPopNextHit(discardedTarget, discardedTime))
      {
        // Spurious hits discarded during preparation delay
      }

      if (currentMillis - lastPrepDispUpdate >= 250)
      {
        lastPrepDispUpdate = currentMillis;
        uint32_t prepSecsLeft = (prepRemainingMs + 999) / 1000;
        char prepStr[24];
        snprintf(prepStr, sizeof(prepStr), STR_PREP_COUNTDOWN_FORMAT, static_cast<int>(prepSecsLeft));
        displayUpdate(STR_PREPARE, prepStr);
      }
    }
    else
    {
      if (randomDelayStartTime != 0)
      {
        sensorIsrReset();
        randomDelayStartTime = 0;
        displayUpdate(STR_PREPARE, STR_WAIT_FOR_BEEP);
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
    }

    if (roundStateGetStartDelayRemainingMs(currentState, delayEndTime, currentMillis) == 0)
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
    processHits();

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
      if (recordedHitCount > 0)
      {
        finishRound(recordedHits[0].splitTime, "Reaction Hit", currentMillis, 0);
        return;
      }
      for (int i = 0; i < MAX_TARGETS; i++)
      {
        if (acceptedHits[i])
        {
          unsigned long lastStop = getLastStopPlateTime();
          unsigned long elapsed = currentMillis - startTime;
          unsigned long finalTime = (lastStop > 0) ? lastStop : elapsed;
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
        : acceptedHits[0];

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
      snprintf(runningTimeStr, sizeof(runningTimeStr), STR_RUNNING_TIME_VALUE_FORMAT, secs, ms);

      bool hitMask[MAX_TARGETS] = {false};
      for (int i = 0; i < MAX_TARGETS; i++)
      {
        hitMask[i] = acceptedHits[i];
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
        buzzerPlay(BuzzerPattern::Confirm, currentMillis);
        Serial.println("[BTN] Reset button held (Long Press) - Round Reset");
        inputCooldownUntil = currentMillis + INPUT_COOLDOWN_MS;
        return;
      }
    }

    if (btnReset->wasReleased() && !resetLongHandled && resetHoldActive)
    {
      resetHoldActive = false;
      if (!submitFinishedResult())
      {
        inputCooldownUntil = currentMillis + INPUT_COOLDOWN_MS;
        return;
      }
      buzzerPlay(BuzzerPattern::Confirm, currentMillis);
      Serial.print("[BTN] Reset button short press - Round Scored: ");
      Serial.print(endTime);
      Serial.print(" ms, Total Penalties/Missed: ");
      Serial.println(penaltyCount);
      resetToStandby();
      inputCooldownUntil = currentMillis + INPUT_COOLDOWN_MS;
      return;
    }

    // An uncertain commit may already contain this physical confirmation.
    // Keep its fields unchanged until recovery; explicit long Reset still discards RAM.
    if (confirmationFailed && offlineBufferHasStorageError())
    {
      return;
    }

    if (startPressed)
    {
      if (menuGetRoundMode() == 2)
      {
        // Confirm the previous reaction result before immediately starting the next attempt.
        if (!submitFinishedResult())
        {
          inputCooldownUntil = currentMillis + INPUT_COOLDOWN_MS;
          return;
        }
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
        if (menuGetRoundMode() == 2)
        {
          return;
        }
        int minPenalty = automaticMissCount;
        penaltyCount = minPenalty;
        updateResultScore();
        displayCycleIndex = 0;
        buzzerPlay(BuzzerPattern::DoubleShort, currentMillis);
        updateFinishedDisplay();
        Serial.println("[BTN] Red button held (Long Press) - Reset penalties to min missed targets");
      }
    }

    if (btnStop->wasReleased() && !stopLongHandled && stopHoldActive)
    {
      stopHoldActive = false;
      if (menuGetRoundMode() == 2)
      {
        return;
      }
      int minPenalty = automaticMissCount;
      penaltyCount++;
      if (penaltyCount > 10)
      {
        penaltyCount = minPenalty;
      }
      if (penaltyCount < minPenalty)
      {
        penaltyCount = minPenalty;
      }
      updateResultScore();
      displayCycleIndex = 0;

      buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
      updateFinishedDisplay();
    }


    // Keep pending results visible until physical confirmation or discard.
    break;

  case MENU:
    lastActivityTime = currentMillis;
    break;
  }
}

State roundControllerGetState()
{
  return currentState;
}

int roundControllerGetRoundMode()
{
  return menuGetRoundMode();
}

bool roundControllerSetRoundMode(int mode)
{
  if (currentState != STANDBY || mode < 0 || mode > 2)
  {
    return false;
  }
  menuSetRoundMode(mode);
  storageSaveRoundMode(mode);
  renderStandbyScreen();
  return true;
}

unsigned long roundControllerGetElapsedMs(unsigned long currentMillis)
{
  if (currentState == RUNNING)
  {
    if (!menuGetRequireAllTargets() && isStopPlateHitLast())
    {
      unsigned long lastStop = getLastStopPlateTime();
      if (lastStop > 0)
      {
        return lastStop;
      }
    }
    return static_cast<long>(currentMillis - startTime) >= 0 ? (currentMillis - startTime) : 0;
  }
  else if (currentState == FINISHED)
  {
    return endTime;
  }
  else if (currentState == STANDBY)
  {
    return lastRoundTimeMs;
  }
  return 0;
}

unsigned long roundControllerGetCountdownId()
{
  return countdownId;
}

unsigned long roundControllerGetStartDelayRemainingMs(unsigned long currentMillis)
{
  if (currentState != DELAY)
  {
    return 0;
  }

  return roundStateGetStartDelayRemainingMs(currentState, delayEndTime, currentMillis);
}

int roundControllerGetPreparationDelay()
{
  return menuGetPreparationDelay();
}

bool roundControllerSetPreparationDelay(int seconds)
{
  if ((currentState != STANDBY && currentState != MENU) || seconds < 0 || seconds > 30)
  {
    return false;
  }
  menuSetPreparationDelay(seconds);
  storageSavePreparationDelay(seconds);
  return true;
}

unsigned long roundControllerGetPrepDelayRemainingMs(unsigned long currentMillis)
{
  if (currentState != DELAY || randomDelayStartTime == 0)
  {
    return 0;
  }
  return roundStateGetPrepDelayRemainingMs(currentState, randomDelayStartTime, currentMillis);
}

int roundControllerGetPenaltyCount()
{
  return penaltyCount;
}

int roundControllerGetAutomaticMissCount()
{
  return automaticMissCount;
}

void roundControllerGetHitMask(bool* outMask, int maxTargets)
{
  if (!outMask)
  {
    return;
  }
  int limit = maxTargets < MAX_TARGETS ? maxTargets : MAX_TARGETS;
  for (int i = 0; i < limit; i++)
  {
    outMask[i] = acceptedHits[i];
  }
}

int roundControllerGetRecordedHits(int* outTargetIds, unsigned long* outSplitTimes, int maxHits)
{
  if (!outTargetIds || !outSplitTimes)
  {
    return 0;
  }
  int count = recordedHitCount < maxHits ? recordedHitCount : maxHits;
  for (int i = 0; i < count; i++)
  {
    outTargetIds[i] = recordedHits[i].targetId;
    outSplitTimes[i] = recordedHits[i].splitTime;
  }
  return count;
}

bool roundControllerGetLatestTrainingResult(TrainingResult &result)
{
  return trainingResultGet(result);
}
