#include <unity.h>
#include "round_result.h"
#include "round_result.cpp"

void setUp(void)
{
    roundResultDiscard();
}

void tearDown(void) {}

void test_result_is_consumed_exactly_once(void)
{
    roundResultPrepare(1250, 4250, 1, 1);

    RoundResult result = {};
    TEST_ASSERT_TRUE(roundResultTake(result));
    TEST_ASSERT_EQUAL_UINT32(1250, result.rawTimeMs);
    TEST_ASSERT_EQUAL_UINT32(4250, result.timeMs);
    TEST_ASSERT_EQUAL_INT(1, result.missCount);
    TEST_ASSERT_EQUAL_INT(1, result.penaltyCount);
    TEST_ASSERT_FALSE(roundResultTake(result));
}

void test_penalty_changes_update_pending_result(void)
{
    roundResultPrepare(1250, 4250, 1, 1);
    roundResultUpdate(1250, 7250, 1, 2);

    RoundResult result = {};
    TEST_ASSERT_TRUE(roundResultTake(result));
    TEST_ASSERT_EQUAL_UINT32(1250, result.rawTimeMs);
    TEST_ASSERT_EQUAL_UINT32(7250, result.timeMs);
    TEST_ASSERT_EQUAL_INT(1, result.missCount);
    TEST_ASSERT_EQUAL_INT(2, result.penaltyCount);
}

void test_discard_prevents_submission(void)
{
    roundResultPrepare(1250, 4250, 1, 1);
    roundResultDiscard();

    RoundResult result = {};
    TEST_ASSERT_FALSE(roundResultTake(result));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_result_is_consumed_exactly_once);
    RUN_TEST(test_penalty_changes_update_pending_result);
    RUN_TEST(test_discard_prevents_submission);
    return UNITY_END();
}
