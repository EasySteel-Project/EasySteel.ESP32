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

void test_reaction_drill_always_scores_zero_missed_targets(void)
{
    bool emptyMask[MAX_TARGETS] = {};
    TEST_ASSERT_EQUAL_INT(0, scoringRulesCalculateMissedTargets(5, emptyMask, 2));

    bool singleHitMask[MAX_TARGETS] = {};
    singleHitMask[2] = true;
    TEST_ASSERT_EQUAL_INT(0, scoringRulesCalculateMissedTargets(5, singleHitMask, 2));

    bool stopHitMask[MAX_TARGETS] = {};
    stopHitMask[0] = true;
    TEST_ASSERT_EQUAL_INT(0, scoringRulesCalculateMissedTargets(5, stopHitMask, 2));
}

void test_final_score_caps_time_without_overflow()
{
    TEST_ASSERT_EQUAL(4250, scoringRulesFinalTime(1250, 1));
    TEST_ASSERT_EQUAL(30000, scoringRulesFinalTime(29000, 1));
    TEST_ASSERT_EQUAL(30000, scoringRulesFinalTime(30010, 0));
    TEST_ASSERT_EQUAL(30000, scoringRulesFinalTime(1000, 2147483647));
    TEST_ASSERT_EQUAL(1000, scoringRulesFinalTime(1000, -1));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_final_score_caps_time_without_overflow);
    RUN_TEST(test_unconfigured_hits_do_not_reduce_missed_target_count);
    RUN_TEST(test_missed_target_count_handles_supported_boundaries);
    RUN_TEST(test_only_configured_targets_are_recorded_outside_reaction_drill);
    RUN_TEST(test_reaction_drill_always_scores_zero_missed_targets);
    return UNITY_END();
}
