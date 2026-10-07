#include <unity.h>
#include "Arduino.h"
#include "buzzer.h"
#include "config.h"
#include "mock_globals.cpp"

static int buzzerOutput = LOW;

static void recordBuzzerOutput(int pin, int value)
{
    if (pin == BUZZER_PIN)
    {
        buzzerOutput = value;
    }
}

#define digitalWrite recordBuzzerOutput
#include "buzzer.cpp"
#undef digitalWrite

void setUp(void)
{
    buzzerInit();
    buzzerPlay(BuzzerPattern::None, 0);
}

void tearDown(void) {}

void test_setup_confirmation_finishes_two_short_beeps_in_150_ms(void)
{
    buzzerPlay(BuzzerPattern::Confirm, 1000);
    TEST_ASSERT_EQUAL_INT(HIGH, buzzerOutput);
    buzzerTick(1049);
    TEST_ASSERT_EQUAL_INT(HIGH, buzzerOutput);
    buzzerTick(1050);
    TEST_ASSERT_EQUAL_INT(LOW, buzzerOutput);
    buzzerTick(1099);
    TEST_ASSERT_EQUAL_INT(LOW, buzzerOutput);
    buzzerTick(1100);
    TEST_ASSERT_EQUAL_INT(HIGH, buzzerOutput);
    buzzerTick(1149);
    TEST_ASSERT_EQUAL_INT(HIGH, buzzerOutput);
    buzzerTick(1150);
    TEST_ASSERT_EQUAL_INT(LOW, buzzerOutput);
    buzzerTick(2000);
    TEST_ASSERT_EQUAL_INT(LOW, buzzerOutput);
}

void test_round_start_keeps_one_second_signal(void)
{
    buzzerPlay(BuzzerPattern::Start, 1000);
    buzzerTick(1999);
    TEST_ASSERT_EQUAL_INT(HIGH, buzzerOutput);
    buzzerTick(2000);
    TEST_ASSERT_EQUAL_INT(LOW, buzzerOutput);
}

void test_simple_button_click_finishes_immediately_and_cancels_previous_tone(void)
{
    g_mockMillis = 1000;
    buzzerPlay(BuzzerPattern::Confirm, 1000);
    buzzerPlay(BuzzerPattern::MenuClick, 1000);
    TEST_ASSERT_EQUAL_INT(LOW, buzzerOutput);
    TEST_ASSERT_EQUAL_UINT32(1002, g_mockMillis);
    buzzerTick(1100);
    TEST_ASSERT_EQUAL_INT(LOW, buzzerOutput);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_setup_confirmation_finishes_two_short_beeps_in_150_ms);
    RUN_TEST(test_round_start_keeps_one_second_signal);
    RUN_TEST(test_simple_button_click_finishes_immediately_and_cancels_previous_tone);
    return UNITY_END();
}
