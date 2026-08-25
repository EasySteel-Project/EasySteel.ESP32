#include <unity.h>
#include "sensor_isr.h"
#include "sensor_isr.cpp"
#include "Arduino.h"
#include "mock_globals.cpp"

// Expose internal isr handler for testing
void onSensorHitIdx(int i);

void setUp(void) {
    g_mockMillis = 100;
    sensorIsrReset();
    sensorIsrResetLastAnyHitTime();
}

void tearDown(void) {}

void test_isr_ignored_when_not_running(void) {
    sensorIsrSetRunning(false);
    onSensorHitIdx(0);

    TEST_ASSERT_FALSE(sensorIsrWasHit(0));
}

void test_isr_valid_hit_when_running(void) {
    sensorIsrSetRunning(true);
    onSensorHitIdx(0);

    TEST_ASSERT_TRUE(sensorIsrWasHit(0));

    int targetIndex = -1;
    unsigned long hitTime = 0;
    TEST_ASSERT_TRUE(sensorIsrPopNextHit(targetIndex, hitTime));
    TEST_ASSERT_EQUAL_INT(0, targetIndex);
    TEST_ASSERT_EQUAL(100, hitTime);
}

void test_isr_per_target_debounce_50ms(void) {
    sensorIsrSetRunning(true);
    onSensorHitIdx(1);

    // Advance time by only 30ms (< 50ms debounce)
    g_mockMillis = 130;
    onSensorHitIdx(1);

    // Reset hit status to test queue
    int targetIndex = -1;
    unsigned long hitTime = 0;
    TEST_ASSERT_TRUE(sensorIsrPopNextHit(targetIndex, hitTime));
    TEST_ASSERT_EQUAL_INT(1, targetIndex);
    TEST_ASSERT_EQUAL(100, hitTime); // First hit time recorded, second ignored

    TEST_ASSERT_FALSE(sensorIsrPopNextHit(targetIndex, hitTime)); // No second hit queued
}

void test_isr_global_crosstalk_25ms(void) {
    sensorIsrSetRunning(true);
    g_mockMillis = 500;
    onSensorHitIdx(0); // Hit target 0 at 500ms

    // Advance time by only 10ms (< 25ms crosstalk dead-time)
    g_mockMillis = 510;
    onSensorHitIdx(1); // Crosstalk on target 1

    TEST_ASSERT_FALSE(sensorIsrWasHit(1)); // Target 1 hit ignored due to crosstalk
}

void test_isr_test_mode_allows_multiple_hits_on_intermediate_targets(void) {
    sensorIsrSetRunning(true);
    sensorIsrSetTestMode(true);

    // Hit intermediate target 3 (Target 4) at 100ms
    g_mockMillis = 100;
    onSensorHitIdx(3);

    int targetIndex = -1;
    unsigned long hitTime = 0;
    TEST_ASSERT_TRUE(sensorIsrPopNextHit(targetIndex, hitTime));
    TEST_ASSERT_EQUAL_INT(3, targetIndex);
    TEST_ASSERT_EQUAL(100, hitTime);

    // In normal mode, target 3 hit would be ignored. In test mode (after debounce 50ms), hit target 3 again at 200ms
    g_mockMillis = 200;
    onSensorHitIdx(3);

    TEST_ASSERT_TRUE(sensorIsrPopNextHit(targetIndex, hitTime));
    TEST_ASSERT_EQUAL_INT(3, targetIndex);
    TEST_ASSERT_EQUAL(200, hitTime);

    sensorIsrSetTestMode(false);
}

void test_queue_preserves_chronological_hit_order(void) {
    sensorIsrSetRunning(true);
    sensorIsrSetTestMode(true);

    g_mockMillis = 100;
    onSensorHitIdx(4);
    g_mockMillis = 130;
    onSensorHitIdx(1);
    g_mockMillis = 160;
    onSensorHitIdx(0);

    int targetIndex = -1;
    unsigned long hitTime = 0;
    TEST_ASSERT_TRUE(sensorIsrPopNextHit(targetIndex, hitTime));
    TEST_ASSERT_EQUAL_INT(4, targetIndex);
    TEST_ASSERT_EQUAL_UINT32(100, hitTime);
    TEST_ASSERT_TRUE(sensorIsrPopNextHit(targetIndex, hitTime));
    TEST_ASSERT_EQUAL_INT(1, targetIndex);
    TEST_ASSERT_EQUAL_UINT32(130, hitTime);
    TEST_ASSERT_TRUE(sensorIsrPopNextHit(targetIndex, hitTime));
    TEST_ASSERT_EQUAL_INT(0, targetIndex);
    TEST_ASSERT_EQUAL_UINT32(160, hitTime);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_isr_ignored_when_not_running);
    RUN_TEST(test_isr_valid_hit_when_running);
    RUN_TEST(test_isr_per_target_debounce_50ms);
    RUN_TEST(test_isr_global_crosstalk_25ms);
    RUN_TEST(test_isr_test_mode_allows_multiple_hits_on_intermediate_targets);
    RUN_TEST(test_queue_preserves_chronological_hit_order);
    return UNITY_END();
}
