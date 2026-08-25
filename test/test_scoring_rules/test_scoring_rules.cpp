#include <unity.h>
#include "scoring_rules.h"
#include "scoring_rules.cpp"

void setUp(void) {}
void tearDown(void) {}

void test_unconfigured_hits_do_not_reduce_missed_target_count(void)
{
    bool hitMask[MAX_TARGETS] = {};
    hitMask[1] = true;
    hitMask[3] = true;
    hitMask[4] = true;

    TEST_ASSERT_EQUAL_INT(1, scoringRulesCalculateMissedTargets(3, hitMask));
}

void test_missed_target_count_handles_supported_boundaries(void)
{
    bool hitMask[MAX_TARGETS] = {};
    TEST_ASSERT_EQUAL_INT(0, scoringRulesCalculateMissedTargets(1, hitMask));
    TEST_ASSERT_EQUAL_INT(4, scoringRulesCalculateMissedTargets(5, hitMask));

    hitMask[1] = true;
    hitMask[2] = true;
    hitMask[3] = true;
    hitMask[4] = true;
    TEST_ASSERT_EQUAL_INT(0, scoringRulesCalculateMissedTargets(5, hitMask));
}

void test_only_configured_targets_are_recorded_outside_reaction_drill(void)
{
    TEST_ASSERT_TRUE(scoringRulesShouldRecordTarget(0, 3, 0));
    TEST_ASSERT_TRUE(scoringRulesShouldRecordTarget(2, 3, 0));
    TEST_ASSERT_FALSE(scoringRulesShouldRecordTarget(3, 3, 0));
    TEST_ASSERT_TRUE(scoringRulesShouldRecordTarget(4, 3, 2));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_unconfigured_hits_do_not_reduce_missed_target_count);
    RUN_TEST(test_missed_target_count_handles_supported_boundaries);
    RUN_TEST(test_only_configured_targets_are_recorded_outside_reaction_drill);
    return UNITY_END();
}
