#include <unity.h>
#include "remote_control.h"
#include "remote_control.cpp"

void setUp(void)
{
  srand(1);
  remoteControlInit();
}

void tearDown(void) {}

static void pairPhone(unsigned long currentMillis, char *token, size_t tokenSize)
{
  TEST_ASSERT_TRUE(remoteControlBeginPairing(currentMillis));
  const char *pin = remoteControlGetPairingPin();
  TEST_ASSERT_EQUAL_UINT32(4, strlen(pin));
  TEST_ASSERT_EQUAL(
    RemotePairResult::Success,
    remoteControlPair(pin, currentMillis, token, tokenSize));
}

void test_pairing_requires_hardware_window_and_valid_pin(void)
{
  char token[REMOTE_SESSION_TOKEN_LENGTH + 1] = "";
  TEST_ASSERT_EQUAL(
    RemotePairResult::PairingUnavailable,
    remoteControlPair("1234", 1000, token, sizeof(token)));

  TEST_ASSERT_TRUE(remoteControlBeginPairing(1000));
  TEST_ASSERT_EQUAL(RemoteControlState::Pairing, remoteControlGetState());
  TEST_ASSERT_EQUAL(
    RemotePairResult::InvalidPin,
    remoteControlPair("xxxx", 1000, token, sizeof(token)));

  char shortToken[REMOTE_SESSION_TOKEN_LENGTH] = "";
  const char *pin = remoteControlGetPairingPin();
  TEST_ASSERT_EQUAL(
    RemotePairResult::PairingUnavailable,
    remoteControlPair(pin, 1000, shortToken, sizeof(shortToken)));

  TEST_ASSERT_EQUAL(
    RemotePairResult::Success,
    remoteControlPair(pin, 1000, token, sizeof(token)));
  TEST_ASSERT_EQUAL_UINT32(REMOTE_SESSION_TOKEN_LENGTH, strlen(token));
  TEST_ASSERT_TRUE(remoteControlAuthorize(token, 1000));
}

void test_pairing_expires_and_five_failures_close_window(void)
{
  char token[REMOTE_SESSION_TOKEN_LENGTH + 1] = "";
  TEST_ASSERT_TRUE(remoteControlBeginPairing(1000));
  remoteControlTick(1000 + REMOTE_PAIRING_TIMEOUT_MS);
  TEST_ASSERT_EQUAL(RemoteControlState::Disabled, remoteControlGetState());

  TEST_ASSERT_TRUE(remoteControlBeginPairing(2000));
  for (int attempt = 0; attempt < REMOTE_PAIRING_MAX_ATTEMPTS - 1; attempt++)
  {
    TEST_ASSERT_EQUAL(
      RemotePairResult::InvalidPin,
      remoteControlPair("xxxx", 2000, token, sizeof(token)));
  }
  TEST_ASSERT_EQUAL(
    RemotePairResult::AttemptsExceeded,
    remoteControlPair("xxxx", 2000, token, sizeof(token)));
  TEST_ASSERT_EQUAL(RemoteControlState::Disabled, remoteControlGetState());
}

void test_only_paired_token_authorizes_and_session_expires(void)
{
  char token[REMOTE_SESSION_TOKEN_LENGTH + 1] = "";
  pairPhone(1000, token, sizeof(token));
  TEST_ASSERT_FALSE(remoteControlAuthorize("wrong", 2000));
  TEST_ASSERT_TRUE(remoteControlAuthorize(token, 2000));
  TEST_ASSERT_TRUE(remoteControlAuthorize(token, 2000 + REMOTE_SESSION_TIMEOUT_MS - 1));

  remoteControlTick(2000 + (2 * REMOTE_SESSION_TIMEOUT_MS));
  TEST_ASSERT_EQUAL(RemoteControlState::Disabled, remoteControlGetState());
  TEST_ASSERT_FALSE(remoteControlAuthorize(token, 2000 + (2 * REMOTE_SESSION_TIMEOUT_MS)));
}

void test_pc_connection_revokes_phone_and_requires_new_hardware_pairing(void)
{
  char token[REMOTE_SESSION_TOKEN_LENGTH + 1] = "";
  pairPhone(1000, token, sizeof(token));

  remoteControlSetPcConnected(true);
  TEST_ASSERT_EQUAL(RemoteControlState::BlockedByPc, remoteControlGetState());
  TEST_ASSERT_FALSE(remoteControlAuthorize(token, 1000));
  TEST_ASSERT_FALSE(remoteControlBeginPairing(1000));

  remoteControlSetPcConnected(false);
  TEST_ASSERT_EQUAL(RemoteControlState::Disabled, remoteControlGetState());
  TEST_ASSERT_FALSE(remoteControlAuthorize(token, 1000));
  TEST_ASSERT_TRUE(remoteControlBeginPairing(1000));
}

void test_command_slot_is_bounded_and_consumed_once(void)
{
  char token[REMOTE_SESSION_TOKEN_LENGTH + 1] = "";
  pairPhone(1000, token, sizeof(token));
  TEST_ASSERT_TRUE(remoteControlQueueCommand(RemoteControlAction::Start));
  TEST_ASSERT_FALSE(remoteControlQueueCommand(RemoteControlAction::Stop));

  RemoteControlCommand command = {RemoteControlAction::Reset, 0};
  TEST_ASSERT_TRUE(remoteControlTakeCommand(command));
  TEST_ASSERT_EQUAL(RemoteControlAction::Start, command.action);
  TEST_ASSERT_FALSE(remoteControlTakeCommand(command));

  TEST_ASSERT_TRUE(remoteControlQueueCommand(RemoteControlAction::SetMode, 2));
  remoteControlClearPendingCommand();
  TEST_ASSERT_FALSE(remoteControlTakeCommand(command));
}

void test_action_policy_matches_solo_training_controls(void)
{
  TEST_ASSERT_TRUE(remoteControlActionAllowed(RemoteControlAction::Start, STANDBY));
  TEST_ASSERT_TRUE(remoteControlActionAllowed(RemoteControlAction::Start, FINISHED));
  TEST_ASSERT_FALSE(remoteControlActionAllowed(RemoteControlAction::Start, RUNNING));

  TEST_ASSERT_TRUE(remoteControlActionAllowed(RemoteControlAction::Stop, DELAY));
  TEST_ASSERT_TRUE(remoteControlActionAllowed(RemoteControlAction::Stop, RUNNING));
  TEST_ASSERT_FALSE(remoteControlActionAllowed(RemoteControlAction::Stop, STANDBY));

  TEST_ASSERT_TRUE(remoteControlActionAllowed(RemoteControlAction::Reset, DELAY));
  TEST_ASSERT_TRUE(remoteControlActionAllowed(RemoteControlAction::Reset, RUNNING));
  TEST_ASSERT_TRUE(remoteControlActionAllowed(RemoteControlAction::Reset, FINISHED));
  TEST_ASSERT_FALSE(remoteControlActionAllowed(RemoteControlAction::Reset, STANDBY));

  TEST_ASSERT_TRUE(remoteControlActionAllowed(RemoteControlAction::SetMode, STANDBY, 0));
  TEST_ASSERT_TRUE(remoteControlActionAllowed(RemoteControlAction::SetMode, STANDBY, 2));
  TEST_ASSERT_FALSE(remoteControlActionAllowed(RemoteControlAction::SetMode, STANDBY, -1));
  TEST_ASSERT_FALSE(remoteControlActionAllowed(RemoteControlAction::SetMode, STANDBY, 3));
  TEST_ASSERT_FALSE(remoteControlActionAllowed(RemoteControlAction::SetMode, RUNNING, 1));

  TEST_ASSERT_TRUE(remoteControlActionAllowed(RemoteControlAction::SetPrepDelay, STANDBY, 0));
  TEST_ASSERT_TRUE(remoteControlActionAllowed(RemoteControlAction::SetPrepDelay, STANDBY, 15));
  TEST_ASSERT_TRUE(remoteControlActionAllowed(RemoteControlAction::SetPrepDelay, STANDBY, 30));
  TEST_ASSERT_FALSE(remoteControlActionAllowed(RemoteControlAction::SetPrepDelay, STANDBY, -1));
  TEST_ASSERT_FALSE(remoteControlActionAllowed(RemoteControlAction::SetPrepDelay, STANDBY, 31));
  TEST_ASSERT_FALSE(remoteControlActionAllowed(RemoteControlAction::SetPrepDelay, RUNNING, 10));
}

void test_timeouts_are_millis_rollover_safe(void)
{
  unsigned long startedAt = 0xFFFFFF00UL;
  TEST_ASSERT_TRUE(remoteControlBeginPairing(startedAt));
  remoteControlTick(startedAt + REMOTE_PAIRING_TIMEOUT_MS - 1);
  TEST_ASSERT_EQUAL(RemoteControlState::Pairing, remoteControlGetState());
  remoteControlTick(startedAt + REMOTE_PAIRING_TIMEOUT_MS);
  TEST_ASSERT_EQUAL(RemoteControlState::Disabled, remoteControlGetState());
}

void test_pending_command_is_revoked_by_pc_or_expiry()
{
  char token[REMOTE_SESSION_TOKEN_LENGTH + 1];
  pairPhone(1000, token, sizeof(token));
  RemoteControlCommand command{};
  TEST_ASSERT_TRUE(remoteControlQueueCommand(RemoteControlAction::Start, 0, 1000, STANDBY, 4));
  remoteControlSetPcConnected(true);
  TEST_ASSERT_FALSE(remoteControlTakeCommand(command, 1001, STANDBY, 4));
  remoteControlSetPcConnected(false);
  pairPhone(2000, token, sizeof(token));
  TEST_ASSERT_TRUE(remoteControlQueueCommand(RemoteControlAction::Start, 0, 2000, STANDBY, 4));
  TEST_ASSERT_FALSE(remoteControlTakeCommand(command, 3000, STANDBY, 4));
}

void test_commands_cannot_cross_rounds_but_stop_can_cross_start_signal()
{
  char token[REMOTE_SESSION_TOKEN_LENGTH + 1];
  pairPhone(1000, token, sizeof(token));
  RemoteControlCommand command{};
  TEST_ASSERT_TRUE(remoteControlQueueCommand(RemoteControlAction::Reset, 0, 1000, RUNNING, 4));
  TEST_ASSERT_FALSE(remoteControlTakeCommand(command, 1001, RUNNING, 5));
  TEST_ASSERT_TRUE(remoteControlQueueCommand(RemoteControlAction::Stop, 0, 1100, DELAY, 5));
  TEST_ASSERT_TRUE(remoteControlTakeCommand(command, 1101, RUNNING, 5));
  TEST_ASSERT_EQUAL(RemoteControlAction::Stop, command.action);
  TEST_ASSERT_TRUE(remoteControlQueueCommand(RemoteControlAction::SetMode, 2, 1200, STANDBY, 5));
  TEST_ASSERT_FALSE(remoteControlTakeCommand(command, 1201, RUNNING, 5));
}

void test_hardware_input_and_revocation_invalidate_queued_http_epoch()
{
  unsigned long epoch = remoteControlGetCommandEpoch();
  remoteControlClearPendingCommand();
  TEST_ASSERT_NOT_EQUAL(epoch, remoteControlGetCommandEpoch());
  epoch = remoteControlGetCommandEpoch();
  remoteControlDisable();
  TEST_ASSERT_NOT_EQUAL(epoch, remoteControlGetCommandEpoch());
  TEST_ASSERT_FALSE(remoteControlCommandStillValid(RemoteControlAction::Start, RUNNING, FINISHED, 3, 3));
  TEST_ASSERT_FALSE(remoteControlCommandStillValid(RemoteControlAction::Start, STANDBY, STANDBY, 3, 4));
  TEST_ASSERT_TRUE(remoteControlCommandStillValid(RemoteControlAction::Stop, DELAY, RUNNING, 3, 3));
}

void test_manual_disable_revokes_authorization_and_session(void)
{
  char token[REMOTE_SESSION_TOKEN_LENGTH + 1] = "";
  pairPhone(1000, token, sizeof(token));
  TEST_ASSERT_EQUAL(RemoteControlState::Paired, remoteControlGetState());
  TEST_ASSERT_TRUE(remoteControlAuthorize(token, 1000));

  remoteControlDisable();
  TEST_ASSERT_EQUAL(RemoteControlState::Disabled, remoteControlGetState());
  TEST_ASSERT_FALSE(remoteControlAuthorize(token, 1000));
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();
  RUN_TEST(test_hardware_input_and_revocation_invalidate_queued_http_epoch);
  RUN_TEST(test_pending_command_is_revoked_by_pc_or_expiry);
  RUN_TEST(test_commands_cannot_cross_rounds_but_stop_can_cross_start_signal);
  RUN_TEST(test_pairing_requires_hardware_window_and_valid_pin);
  RUN_TEST(test_pairing_expires_and_five_failures_close_window);
  RUN_TEST(test_only_paired_token_authorizes_and_session_expires);
  RUN_TEST(test_pc_connection_revokes_phone_and_requires_new_hardware_pairing);
  RUN_TEST(test_command_slot_is_bounded_and_consumed_once);
  RUN_TEST(test_action_policy_matches_solo_training_controls);
  RUN_TEST(test_timeouts_are_millis_rollover_safe);
  RUN_TEST(test_manual_disable_revokes_authorization_and_session);
  return UNITY_END();
}
