#pragma once

#include <stddef.h>
#include "round_state.h"

constexpr unsigned long REMOTE_PAIRING_TIMEOUT_MS = 60000;
constexpr unsigned long REMOTE_SESSION_TIMEOUT_MS = 30000;
constexpr int REMOTE_PAIRING_MAX_ATTEMPTS = 5;
constexpr size_t REMOTE_SESSION_TOKEN_LENGTH = 32;

enum class RemoteControlState
{
  Disabled,
  Pairing,
  Paired,
  BlockedByPc
};

enum class RemotePairResult
{
  Success,
  InvalidPin,
  PairingUnavailable,
  AttemptsExceeded
};

enum class RemoteControlAction
{
  Start,
  Stop,
  Reset,
  SetMode
};

struct RemoteControlCommand
{
  RemoteControlAction action;
  int value;
};

/// Initialize the volatile solo-training remote-control session.
void remoteControlInit();

/// Open a physically authorized pairing window and generate a four-digit PIN.
bool remoteControlBeginPairing(unsigned long currentMillis);

/// Return the active four-digit PIN for the hardware display, or an empty string.
const char *remoteControlGetPairingPin();

/// Pair one phone and return its strong session token.
RemotePairResult remoteControlPair(
  const char *pin,
  unsigned long currentMillis,
  char *outToken,
  size_t outTokenSize);

/// Validate and renew the paired phone session.
bool remoteControlAuthorize(const char *token, unsigned long currentMillis);

/// Disable pairing and revoke any paired phone.
void remoteControlDisable();

/// Apply PC priority. Connecting a PC immediately revokes mobile control.
void remoteControlSetPcConnected(bool connected);

/// Advance pairing and session timeouts using rollover-safe arithmetic.
void remoteControlTick(unsigned long currentMillis);

/// Return the current remote-control state.
RemoteControlState remoteControlGetState();

/// Return true while a PC blocks solo-training mobile control.
bool remoteControlIsPcConnected();

/// Return whether a mobile action is valid for the current round state.
bool remoteControlActionAllowed(RemoteControlAction action, State state, int value = 0);

/// Queue one authorized command for the round controller.
bool remoteControlQueueCommand(RemoteControlAction action, int value = 0);

/// Consume the pending command exactly once.
bool remoteControlTakeCommand(RemoteControlCommand &command);

/// Discard any pending mobile command, for example when hardware input wins.
void remoteControlClearPendingCommand();
