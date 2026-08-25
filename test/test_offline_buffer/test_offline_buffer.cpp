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

void test_buffer_initial_empty(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, offlineBufferGetCount());
}

void test_buffer_push_single_and_multiple(void)
{
    offlineBufferPush("run-1", 1200, 0);
    offlineBufferPush("run-2", 1500, 1);
    TEST_ASSERT_EQUAL_UINT32(2, offlineBufferGetCount());
}

void test_buffer_overflow_capacity(void)
{
    for (int index = 0; index < 25; index++)
    {
        char runId[24];
        snprintf(runId, sizeof(runId), "run-%d", index);
        offlineBufferPush(runId, 1000 + index * 10, index % 2);
    }
    TEST_ASSERT_EQUAL_UINT32(20, offlineBufferGetCount());
}

void test_legacy_result_is_removed_only_after_completed_socket_send(void)
{
    offlineBufferPush("run-1", 2000, 0);
    BufferedRunData run = {};
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_EQUAL_STRING("run-1", run.runId);
    TEST_ASSERT_TRUE(offlineBufferMarkOldestQueued(run.runId));
    TEST_ASSERT_EQUAL_UINT32(1, offlineBufferGetCount());

    TEST_ASSERT_TRUE(offlineBufferCompleteOldestSend(run.runId, false));
    TEST_ASSERT_EQUAL_UINT32(0, offlineBufferGetCount());
}

void test_ack_result_is_retained_until_matching_ack(void)
{
    offlineBufferPush("run-1", 2000, 0);
    offlineBufferPush("run-2", 2500, 2);

    BufferedRunData run = {};
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_TRUE(offlineBufferMarkOldestQueued(run.runId));
    TEST_ASSERT_TRUE(offlineBufferCompleteOldestSend(run.runId, true));
    TEST_ASSERT_EQUAL_UINT32(2, offlineBufferGetCount());
    TEST_ASSERT_FALSE(offlineBufferAcknowledge("run-2"));
    TEST_ASSERT_TRUE(offlineBufferAcknowledge("run-1"));
    TEST_ASSERT_EQUAL_UINT32(1, offlineBufferGetCount());
}

void test_reconnect_makes_queued_result_available_again(void)
{
    offlineBufferPush("run-1", 2000, 0);
    BufferedRunData run = {};
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_TRUE(offlineBufferMarkOldestQueued(run.runId));
    TEST_ASSERT_FALSE(offlineBufferPeekOldestUnsent(run));

    offlineBufferMarkAllUnsent();
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_EQUAL_STRING("run-1", run.runId);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_buffer_initial_empty);
    RUN_TEST(test_buffer_push_single_and_multiple);
    RUN_TEST(test_buffer_overflow_capacity);
    RUN_TEST(test_legacy_result_is_removed_only_after_completed_socket_send);
    RUN_TEST(test_ack_result_is_retained_until_matching_ack);
    RUN_TEST(test_reconnect_makes_queued_result_available_again);
    return UNITY_END();
}
