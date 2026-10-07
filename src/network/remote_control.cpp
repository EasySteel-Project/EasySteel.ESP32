#include "remote_control.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ESP32
#include <esp_system.h>
#endif

static RemoteControlState controlState = RemoteControlState::Disabled;
static bool pcConnected = false;
static char pairingPin[5] = "";
static char sessionToken[REMOTE_SESSION_TOKEN_LENGTH + 1] = "";
static unsigned long stateChangedAt = 0;
static unsigned long lastSessionActivityAt = 0;
static int failedPairingAttempts = 0;
static unsigned long commandEpoch = 0;
static bool commandPending = false;
static unsigned long commandQueuedAt = 0;
static State commandExpectedState = STANDBY;
static unsigned long commandCountdownId = 0;
static RemoteControlCommand pendingCommand = {RemoteControlAction::Start, 0};

static unsigned long remoteRandom()
{
#ifdef ESP32
  return static_cast<unsigned long>(esp_random());
#else
  return static_cast<unsigned long>(rand());
#endif
}

static void clearCredentialsAndCommand()
{
  ++commandEpoch;
  pairingPin[0] = '\0';
  sessionToken[0] = '\0';
  failedPairingAttempts = 0;
  commandPending = false;
}

static void generateSessionToken()
{
  unsigned long random0 = remoteRandom();
  unsigned long random1 = remoteRandom();
  unsigned long random2 = remoteRandom();
  unsigned long random3 = remoteRandom();
  snprintf(
    sessionToken,
    sizeof(sessionToken),
    "%08lx%08lx%08lx%08lx",
    random0,
    random1,
    random2,
    random3);
}

void remoteControlInit()
{
  pcConnected = false;
  controlState = RemoteControlState::Disabled;
  stateChangedAt = 0;
  lastSessionActivityAt = 0;
  clearCredentialsAndCommand();
}

bool remoteControlBeginPairing(unsigned long currentMillis)
{
  if (pcConnected)
  {
    return false;
  }

  clearCredentialsAndCommand();
  unsigned long pin = remoteRandom() % 10000UL;
  snprintf(pairingPin, sizeof(pairingPin), "%04lu", pin);
  stateChangedAt = currentMillis;
  controlState = RemoteControlState::Pairing;
  return true;
}

const char *remoteControlGetPairingPin()
{
  return controlState == RemoteControlState::Pairing ? pairingPin : "";
}

RemotePairResult remoteControlPair(
  const char *pin,
  unsigned long currentMillis,
  char *outToken,
  size_t outTokenSize)
{
  remoteControlTick(currentMillis);

  if (controlState != RemoteControlState::Pairing || pin == nullptr || outToken == nullptr ||
      outTokenSize < REMOTE_SESSION_TOKEN_LENGTH + 1)
  {
    return RemotePairResult::PairingUnavailable;
  }

  if (strlen(pin) != 4 || strcmp(pin, pairingPin) != 0)
  {
    failedPairingAttempts++;
    if (failedPairingAttempts >= REMOTE_PAIRING_MAX_ATTEMPTS)
    {
      remoteControlDisable();
      return RemotePairResult::AttemptsExceeded;
    }
    return RemotePairResult::InvalidPin;
  }

  generateSessionToken();
  snprintf(outToken, outTokenSize, "%s", sessionToken);
  pairingPin[0] = '\0';
  failedPairingAttempts = 0;
  lastSessionActivityAt = currentMillis;
  controlState = RemoteControlState::Paired;
  return RemotePairResult::Success;
}

bool remoteControlAuthorize(const char *token, unsigned long currentMillis)
{
  remoteControlTick(currentMillis);
  if (controlState != RemoteControlState::Paired || token == nullptr || token[0] == '\0')
  {
    return false;
  }
  if (strcmp(token, sessionToken) != 0)
  {
    return false;
  }

  lastSessionActivityAt = currentMillis;
  return true;
}

void remoteControlDisable()
{
  clearCredentialsAndCommand();
  controlState = pcConnected ? RemoteControlState::BlockedByPc : RemoteControlState::Disabled;
}

void remoteControlSetPcConnected(bool connected)
{
  if (connected == pcConnected)
  {
    return;
  }

  pcConnected = connected;
  clearCredentialsAndCommand();
  controlState = connected ? RemoteControlState::BlockedByPc : RemoteControlState::Disabled;
}

void remoteControlTick(unsigned long currentMillis)
{
  if (controlState == RemoteControlState::Pairing &&
      currentMillis - stateChangedAt >= REMOTE_PAIRING_TIMEOUT_MS)
  {
    remoteControlDisable();
  }
  else if (controlState == RemoteControlState::Paired &&
           currentMillis - lastSessionActivityAt >= REMOTE_SESSION_TIMEOUT_MS)
  {
    remoteControlDisable();
  }
}

RemoteControlState remoteControlGetState()
{
  return controlState;
}

bool remoteControlIsPcConnected()
{
  return pcConnected;
}

bool remoteControlActionAllowed(RemoteControlAction action, State state, int value)
{
  switch (action)
  {
    case RemoteControlAction::Start:
      return state == STANDBY || state == FINISHED;
    case RemoteControlAction::Stop:
      return state == DELAY || state == RUNNING;
    case RemoteControlAction::Reset:
      return state == DELAY || state == RUNNING || state == FINISHED;
    case RemoteControlAction::SetMode:
      return state == STANDBY && value >= 0 && value <= 2;
    case RemoteControlAction::SetPrepDelay:
      return state == STANDBY && value >= 0 && value <= 30;
    default:
      return false;
  }
}

bool remoteControlQueueCommand(RemoteControlAction action, int value,
  unsigned long currentMillis, State expectedState, unsigned long countdownId)
{
  if (controlState != RemoteControlState::Paired || commandPending)
  {
    return false;
  }

  pendingCommand = {action, value};
  commandQueuedAt = currentMillis;
  commandExpectedState = expectedState;
  commandCountdownId = countdownId;
  commandPending = true;
  return true;
}

bool remoteControlTakeCommand(RemoteControlCommand &command, unsigned long currentMillis,
  State state, unsigned long countdownId)
{
  if (!commandPending)
  {
    return false;
  }

  if (controlState != RemoteControlState::Paired || pcConnected ||
      currentMillis - commandQueuedAt >= 1000 ||
      !remoteControlCommandStillValid(pendingCommand.action, commandExpectedState,
        state, commandCountdownId, countdownId, pendingCommand.value))
  {
    commandPending = false;
    return false;
  }
  command = pendingCommand;
  commandPending = false;
  return true;
}

void remoteControlClearPendingCommand()
{
  ++commandEpoch;
  commandPending = false;
}

void remoteControlCopySessionToken(char *out, size_t capacity)
{
  if (out != nullptr && capacity > 0)
  {
    snprintf(out, capacity, "%s", controlState == RemoteControlState::Paired ? sessionToken : "");
  }
}

unsigned long remoteControlGetCommandEpoch()
{
  return commandEpoch;
}

bool remoteControlCommandStillValid(RemoteControlAction action, State expectedState,
  State state, unsigned long expectedCountdown, unsigned long countdownId, int value)
{
  bool sameState = state == expectedState ||
    (expectedState == DELAY && state == RUNNING &&
      (action == RemoteControlAction::Stop || action == RemoteControlAction::Reset));
  return sameState && expectedCountdown == countdownId &&
    remoteControlActionAllowed(action, expectedState, value) && remoteControlActionAllowed(action, state, value);
}
