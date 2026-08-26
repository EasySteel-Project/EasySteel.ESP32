#include <unity.h>
#include "training_result.h"
#include "training_result.cpp"

void setUp(void)
{
  trainingResultReset();
}

void tearDown(void) {}

void test_no_result_is_available_before_first_completed_run(void)
{
  TrainingResult result = {};
  TEST_ASSERT_FALSE(trainingResultGet(result));
}

void test_completed_run_is_copied_with_stable_unique_id(void)
{
  bool hitMask[MAX_TARGETS] = {true, true, false, false, false};
  int targetIds[2] = {1, 0};
  unsigned long splitTimes[2] = {900, 1250};
  trainingResultPublish(0x12345678UL, 1250, 4250, 0, 5, 1, 1, hitMask, targetIds, splitTimes, 2);

  TrainingResult result = {};
  TEST_ASSERT_TRUE(trainingResultGet(result));
  TEST_ASSERT_EQUAL_STRING("12345678-00000001", result.id);
  TEST_ASSERT_EQUAL_UINT32(1250, result.rawTimeMs);
  TEST_ASSERT_EQUAL_UINT32(4250, result.timeMs);
  TEST_ASSERT_EQUAL(0, result.mode);
  TEST_ASSERT_EQUAL(5, result.targetCount);
  TEST_ASSERT_EQUAL(1, result.missCount);
  TEST_ASSERT_EQUAL(1, result.penaltyCount);
  TEST_ASSERT_TRUE(result.hitMask[0]);
  TEST_ASSERT_TRUE(result.hitMask[1]);
  TEST_ASSERT_EQUAL(2, result.hitCount);
  TEST_ASSERT_EQUAL(1, result.hits[0].targetId);
  TEST_ASSERT_EQUAL_UINT32(1250, result.hits[1].splitTimeMs);

  trainingResultPublish(0x87654321UL, 650, 650, 2, 1, 0, 0, hitMask, targetIds, splitTimes, 1);
  TEST_ASSERT_TRUE(trainingResultGet(result));
  TEST_ASSERT_EQUAL_STRING("87654321-00000002", result.id);
}

void test_hit_count_is_bounded_and_null_arrays_are_safe(void)
{
  bool hitMask[MAX_TARGETS] = {};
  int targetIds[MAX_RECORDED_HITS + 2] = {};
  unsigned long splitTimes[MAX_RECORDED_HITS + 2] = {};
  trainingResultPublish(
    1,
    100,
    100,
    1,
    5,
    0,
    0,
    hitMask,
    targetIds,
    splitTimes,
    MAX_RECORDED_HITS + 2);

  TrainingResult result = {};
  TEST_ASSERT_TRUE(trainingResultGet(result));
  TEST_ASSERT_EQUAL(MAX_RECORDED_HITS, result.hitCount);

  trainingResultPublish(2, 200, 200, 1, 5, 0, 0, hitMask, nullptr, nullptr, 4);
  TEST_ASSERT_TRUE(trainingResultGet(result));
  TEST_ASSERT_EQUAL(0, result.hitCount);
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();
  RUN_TEST(test_no_result_is_available_before_first_completed_run);
  RUN_TEST(test_completed_run_is_copied_with_stable_unique_id);
  RUN_TEST(test_hit_count_is_bounded_and_null_arrays_are_safe);
  return UNITY_END();
}
