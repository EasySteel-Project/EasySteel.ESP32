#include <unity.h>
#include "tcp_connect_attempt.h"

struct FakeSocket
{
  static int openResult;
  static int pollResult;
  static int closes;
  static int polls;
  static int open(uint32_t, uint16_t) { return openResult; }
  static int poll(int) { ++polls; return pollResult; }
  static void close(int) { ++closes; }
};
int FakeSocket::openResult;
int FakeSocket::pollResult;
int FakeSocket::closes;
int FakeSocket::polls;

void setUp()
{
  FakeSocket::openResult = 42;
  FakeSocket::pollResult = 0;
  FakeSocket::closes = 0;
  FakeSocket::polls = 0;
}
void tearDown() {}

void test_pending_connection_times_out_without_repeated_open_or_wait()
{
  TcpConnectAttempt<FakeSocket> attempt;
  attempt.begin(0, 1234, 100);
  TEST_ASSERT_EQUAL_INT(-1, attempt.poll(101, true));
  TEST_ASSERT_TRUE(attempt.pending());
  TEST_ASSERT_EQUAL_INT(-1, attempt.poll(249, true));
  TEST_ASSERT_EQUAL_INT(-1, attempt.poll(250, true));
  TEST_ASSERT_FALSE(attempt.pending());
  TEST_ASSERT_EQUAL_INT(2, FakeSocket::polls);
  TEST_ASSERT_EQUAL_INT(1, FakeSocket::closes);
}

void test_success_transfers_socket_ownership_exactly_once()
{
  TcpConnectAttempt<FakeSocket> attempt;
  attempt.begin(0, 1234, 0);
  FakeSocket::pollResult = 1;
  TEST_ASSERT_EQUAL_INT(42, attempt.poll(1, true));
  TEST_ASSERT_EQUAL_INT(-1, attempt.poll(2, true));
  attempt.cancel();
  TEST_ASSERT_EQUAL_INT(0, FakeSocket::closes);
}

void test_round_start_cancels_even_a_ready_connection()
{
  TcpConnectAttempt<FakeSocket> attempt;
  attempt.begin(0, 1234, 0);
  FakeSocket::pollResult = 1;
  TEST_ASSERT_EQUAL_INT(-1, attempt.poll(1, false));
  attempt.cancel();
  TEST_ASSERT_EQUAL_INT(0, FakeSocket::polls);
  TEST_ASSERT_EQUAL_INT(1, FakeSocket::closes);
}

void test_failure_and_retry_do_not_leak_sockets()
{
  TcpConnectAttempt<FakeSocket> attempt;
  FakeSocket::openResult = -1;
  attempt.begin(0, 1234, 0);
  TEST_ASSERT_FALSE(attempt.pending());
  TEST_ASSERT_EQUAL_INT(0, FakeSocket::closes);
  FakeSocket::openResult = 42;
  attempt.begin(0, 1234, 1);
  FakeSocket::pollResult = -1;
  TEST_ASSERT_EQUAL_INT(-1, attempt.poll(2, true));
  TEST_ASSERT_FALSE(attempt.pending());
  TEST_ASSERT_EQUAL_INT(1, FakeSocket::closes);
  attempt.begin(0, 1234, 3);
  attempt.begin(0, 1234, 4);
  attempt.cancel();
  TEST_ASSERT_EQUAL_INT(3, FakeSocket::closes);
}

void test_timeout_survives_millis_rollover()
{
  TcpConnectAttempt<FakeSocket> attempt;
  attempt.begin(0, 1234, UINT32_MAX - 99);
  TEST_ASSERT_EQUAL_INT(-1, attempt.poll(49, true));
  TEST_ASSERT_TRUE(attempt.pending());
  TEST_ASSERT_EQUAL_INT(-1, attempt.poll(50, true));
  TEST_ASSERT_FALSE(attempt.pending());
  TEST_ASSERT_EQUAL_INT(1, FakeSocket::closes);
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_pending_connection_times_out_without_repeated_open_or_wait);
  RUN_TEST(test_success_transfers_socket_ownership_exactly_once);
  RUN_TEST(test_round_start_cancels_even_a_ready_connection);
  RUN_TEST(test_failure_and_retry_do_not_leak_sockets);
  RUN_TEST(test_timeout_survives_millis_rollover);
  return UNITY_END();
}
