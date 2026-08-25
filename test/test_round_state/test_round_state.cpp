#include <unity.h>
#include "round_state.h"
#include "round_state.cpp"

void setUp(void) {}
void tearDown(void) {}

void test_false_start_transitions_from_delay_and_waits_for_display_period(void)
{
    State state = roundStateOnFalseStart(DELAY);
    TEST_ASSERT_EQUAL_INT(FALSE_START, state);
    TEST_ASSERT_EQUAL_INT(FALSE_START, roundStateAdvanceFalseStart(state, 1000, 2199));
    TEST_ASSERT_EQUAL_INT(STANDBY, roundStateAdvanceFalseStart(state, 1000, 2200));
}

void test_false_start_reset_returns_to_standby_immediately(void)
{
    TEST_ASSERT_TRUE(roundStateAllowsImmediateReset(FALSE_START));
    TEST_ASSERT_TRUE(roundStateAllowsImmediateReset(DELAY));
    TEST_ASSERT_TRUE(roundStateAllowsImmediateReset(RUNNING));
    TEST_ASSERT_FALSE(roundStateAllowsImmediateReset(STANDBY));
    TEST_ASSERT_FALSE(roundStateAllowsImmediateReset(FINISHED));
}

void test_false_start_elapsed_time_is_millis_rollover_safe(void)
{
    const uint32_t startedAt = UINT32_MAX - 599;
    TEST_ASSERT_EQUAL_INT(FALSE_START, roundStateAdvanceFalseStart(FALSE_START, startedAt, 599));
    TEST_ASSERT_EQUAL_INT(STANDBY, roundStateAdvanceFalseStart(FALSE_START, startedAt, 600));
}

void test_false_start_event_does_not_change_unrelated_states(void)
{
    TEST_ASSERT_EQUAL_INT(STANDBY, roundStateOnFalseStart(STANDBY));
    TEST_ASSERT_EQUAL_INT(RUNNING, roundStateOnFalseStart(RUNNING));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_false_start_transitions_from_delay_and_waits_for_display_period);
    RUN_TEST(test_false_start_reset_returns_to_standby_immediately);
    RUN_TEST(test_false_start_elapsed_time_is_millis_rollover_safe);
    RUN_TEST(test_false_start_event_does_not_change_unrelated_states);
    return UNITY_END();
}
