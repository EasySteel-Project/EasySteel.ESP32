#include <unity.h>
#include "tcp_outgoing_queue.h"
#include "tcp_outgoing_queue.cpp"

void setUp(void)
{
    tcpOutgoingQueueClear();
}

void tearDown(void) {}

void test_queue_preserves_partial_message_until_fully_consumed(void)
{
    const char payload[] = "abcdef";
    TEST_ASSERT_TRUE(tcpOutgoingQueuePush(payload, 6, TcpMessageKind::Live));

    TcpOutgoingView view = {};
    TEST_ASSERT_TRUE(tcpOutgoingQueuePeek(view));
    TEST_ASSERT_EQUAL_UINT32(6, view.length);

    TcpOutgoingCompletion completion = {};
    TEST_ASSERT_FALSE(tcpOutgoingQueueConsume(2, completion));
    TEST_ASSERT_TRUE(tcpOutgoingQueuePeek(view));
    TEST_ASSERT_EQUAL_UINT32(4, view.length);
    TEST_ASSERT_EQUAL_MEMORY("cdef", view.data, 4);

    TEST_ASSERT_TRUE(tcpOutgoingQueueConsume(4, completion));
    TEST_ASSERT_EQUAL_INT(
      static_cast<int>(TcpMessageKind::Live),
      static_cast<int>(completion.kind));
    TEST_ASSERT_EQUAL_UINT32(0, tcpOutgoingQueueGetCount());
}

void test_result_completion_retains_metadata(void)
{
    TEST_ASSERT_TRUE(tcpOutgoingQueuePush(
      "result", 6, TcpMessageKind::AcknowledgedResult, "run-42"));

    TcpOutgoingCompletion completion = {};
    TEST_ASSERT_TRUE(tcpOutgoingQueueConsume(6, completion));
    TEST_ASSERT_EQUAL_INT(
      static_cast<int>(TcpMessageKind::AcknowledgedResult),
      static_cast<int>(completion.kind));
    TEST_ASSERT_EQUAL_STRING("run-42", completion.runId);
}

void test_queue_has_bounded_capacity(void)
{
    for (int index = 0; index < 16; index++)
        TEST_ASSERT_TRUE(tcpOutgoingQueuePush("x", 1, TcpMessageKind::Live));

    TEST_ASSERT_FALSE(tcpOutgoingQueuePush("x", 1, TcpMessageKind::Live));
    TEST_ASSERT_EQUAL_UINT32(16, tcpOutgoingQueueGetCount());
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_queue_preserves_partial_message_until_fully_consumed);
    RUN_TEST(test_result_completion_retains_metadata);
    RUN_TEST(test_queue_has_bounded_capacity);
    return UNITY_END();
}
