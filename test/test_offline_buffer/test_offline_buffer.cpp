#include <unity.h>
#include "offline_buffer.h"
#include "offline_buffer.cpp"
#include "Arduino.h"
#include "mock_globals.cpp"

void setUp(void)
{
    g_mockMillis = 0;
    offlineBufferClear();
}

void tearDown(void) {}

static bool pushRun(
    const char *runId,
    unsigned long rawTimeMs,
    unsigned long timeMs,
    int missCount,
    int penaltyCount,
    const char *contextId = "")
{
    return offlineBufferPush(
        runId,
        contextId,
        rawTimeMs,
        timeMs,
        missCount,
        penaltyCount);
}

void test_buffer_initial_empty(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, offlineBufferGetCount());
}

void test_buffer_push_single_and_multiple(void)
{
    pushRun("run-1", 1200, 1200, 0, 0);
    pushRun("run-2", 1500, 4500, 1, 1, "stage-2");
    TEST_ASSERT_EQUAL_UINT32(2, offlineBufferGetCount());
}

void test_buffer_overflow_capacity(void)
{
    for (int index = 0; index < 25; index++)
    {
        char runId[24];
        snprintf(runId, sizeof(runId), "run-%d", index);
        pushRun(runId, 1000 + index * 10, 1000 + index * 10, 0, index % 2);
    }
    TEST_ASSERT_EQUAL_UINT32(20, offlineBufferGetCount());
}

void test_result_is_retained_after_completed_socket_send(void)
{
    pushRun("run-1", 2000, 2000, 0, 0);
    BufferedRunData run = {};
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_EQUAL_STRING("run-1", run.runId);
    TEST_ASSERT_TRUE(offlineBufferMarkOldestQueued(run.runId));
    TEST_ASSERT_EQUAL_UINT32(1, offlineBufferGetCount());

    TEST_ASSERT_TRUE(offlineBufferCompleteOldestSend(run.runId, 100));
    TEST_ASSERT_EQUAL_UINT32(1, offlineBufferGetCount());
}

void test_ack_result_is_retained_until_matching_ack(void)
{
    pushRun("run-1", 2000, 5000, 1, 1);
    pushRun("run-2", 2500, 8500, 1, 2);

    BufferedRunData run = {};
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_TRUE(offlineBufferMarkOldestQueued(run.runId));
    TEST_ASSERT_TRUE(offlineBufferCompleteOldestSend(run.runId, 100));
    TEST_ASSERT_EQUAL_UINT32(2, offlineBufferGetCount());
    TEST_ASSERT_FALSE(offlineBufferAcknowledge("run-2"));
    TEST_ASSERT_TRUE(offlineBufferAcknowledge("run-1"));
    TEST_ASSERT_EQUAL_UINT32(1, offlineBufferGetCount());
}

void test_reconnect_makes_queued_result_available_again(void)
{
    pushRun("run-1", 2000, 2000, 0, 0);
    BufferedRunData run = {};
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_TRUE(offlineBufferMarkOldestQueued(run.runId));
    TEST_ASSERT_FALSE(offlineBufferPeekOldestUnsent(run));

    offlineBufferMarkAllUnsent();
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_EQUAL_STRING("run-1", run.runId);
}

void test_result_data_is_preserved_for_retransmission(void)
{
    pushRun("run-1", 1250, 4250, 1, 2, "match-17:stage-3");

    BufferedRunData run = {};
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_EQUAL_STRING("run-1", run.runId);
    TEST_ASSERT_EQUAL_STRING("match-17:stage-3", run.contextId);
    TEST_ASSERT_EQUAL_UINT32(1250, run.rawTimeMs);
    TEST_ASSERT_EQUAL_UINT32(4250, run.timeMs);
    TEST_ASSERT_EQUAL_INT(1, run.missCount);
    TEST_ASSERT_EQUAL_INT(2, run.penaltyCount);
}

void test_ack_timeout_releases_result_and_late_ack_is_accepted(void)
{
    pushRun("run-1", 1250, 4250, 1, 1);
    BufferedRunData run = {};
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_TRUE(offlineBufferMarkOldestQueued(run.runId));
    TEST_ASSERT_TRUE(offlineBufferCompleteOldestSend(run.runId, 100));

    TEST_ASSERT_FALSE(offlineBufferReleaseTimedOutAck(3099, 3000));
    TEST_ASSERT_TRUE(offlineBufferReleaseTimedOutAck(3100, 3000));
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_TRUE(offlineBufferAcknowledge("run-1"));
    TEST_ASSERT_EQUAL_UINT32(0, offlineBufferGetCount());
}

void test_ack_timeout_is_millis_rollover_safe(void)
{
    pushRun("run-1", 1000, 1000, 0, 0);
    BufferedRunData run = {};
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_TRUE(offlineBufferMarkOldestQueued(run.runId));
    TEST_ASSERT_TRUE(offlineBufferCompleteOldestSend(run.runId, 0xfffffff0UL));

    TEST_ASSERT_FALSE(offlineBufferReleaseTimedOutAck(0x00000ba7UL, 3000));
    TEST_ASSERT_TRUE(offlineBufferReleaseTimedOutAck(0x00000ba8UL, 3000));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_buffer_initial_empty);
    RUN_TEST(test_buffer_push_single_and_multiple);
    RUN_TEST(test_buffer_overflow_capacity);
    RUN_TEST(test_result_is_retained_after_completed_socket_send);
    RUN_TEST(test_ack_result_is_retained_until_matching_ack);
    RUN_TEST(test_reconnect_makes_queued_result_available_again);
    RUN_TEST(test_result_data_is_preserved_for_retransmission);
    RUN_TEST(test_ack_timeout_releases_result_and_late_ack_is_accepted);
    RUN_TEST(test_ack_timeout_is_millis_rollover_safe);
    return UNITY_END();
}
