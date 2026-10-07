#include <unity.h>
#include "button.h"
#include "button.cpp"
#include "Arduino.h"
#include "mock_globals.cpp"

void setUp(void) {
    g_mockMillis = 0;
    g_mockPinState = HIGH; // Unpressed for activeLow button
}

void tearDown(void) {}

void test_button_initial_state(void) {
    Button btn(12, true, true); // Pin 12, pullUp=true, activeLow=true
    btn.init();
    TEST_ASSERT_FALSE(btn.isDown());
}

void test_button_press_and_debounce(void) {
    Button btn(12, true, true);
    btn.init();

    // Button pressed (LOW)
    g_mockPinState = LOW;
    btn.update(10);
    // Before 50ms debounce threshold, should not register as down yet
    TEST_ASSERT_FALSE(btn.isDown());

    // Advance time past DEBOUNCE_DELAY_MS (50ms)
    btn.update(70);
    TEST_ASSERT_TRUE(btn.isDown());
    TEST_ASSERT_TRUE(btn.wasPressed());
}

void test_button_long_press_held(void) {
    Button btn(12, true, true);
    btn.init();

    // Press button (raw pin LOW at t=10)
    g_mockPinState = LOW;
    btn.update(10);
    // Debounced DOWN at t=70 (_pressStartTime = 70)
    btn.update(70);
    TEST_ASSERT_TRUE(btn.isDown());

    // Check held for 1500ms at t=1600 (1600 - 70 = 1530ms >= 1500ms)
    btn.update(1600);
    TEST_ASSERT_FALSE(btn.isHeldFor(1500, 1000));
    TEST_ASSERT_TRUE(btn.isHeldFor(1500, 1600));
}

void test_button_release(void) {
    Button btn(12, true, true);
    btn.init();

    // Press button & debounce
    g_mockPinState = LOW;
    btn.update(10);
    btn.update(70);
    TEST_ASSERT_TRUE(btn.isDown());

    // Release button (HIGH at t=80)
    g_mockPinState = HIGH;
    btn.update(80);
    TEST_ASSERT_TRUE(btn.isDown()); // still waiting for debounce

    // Debounced RELEASE at t=140
    btn.update(140);
    TEST_ASSERT_FALSE(btn.isDown());
    TEST_ASSERT_TRUE(btn.wasReleased());
}

void test_consumed_menu_entry_press_does_not_trigger_hold_or_release(void)
{
    Button btn(12, true, true);
    btn.init();
    g_mockPinState = LOW;
    btn.update(10);
    btn.update(70);
    TEST_ASSERT_TRUE(btn.isHeldFor(1500, 1570));

    btn.consumeCurrentPress();
    TEST_ASSERT_TRUE(btn.isDown());
    TEST_ASSERT_FALSE(btn.wasPressed());
    TEST_ASSERT_FALSE(btn.isHeldFor(1000, 1571));
    TEST_ASSERT_FALSE(btn.isHeldFor(1000, 5000));

    g_mockPinState = HIGH;
    btn.update(5010);
    btn.update(5070);
    TEST_ASSERT_FALSE(btn.isDown());
    TEST_ASSERT_FALSE(btn.wasReleased());

    // A fresh press must still support menu navigation and long-press exit.
    g_mockPinState = LOW;
    btn.update(5100);
    btn.update(5160);
    TEST_ASSERT_TRUE(btn.wasPressed());
    TEST_ASSERT_FALSE(btn.isHeldFor(1000, 6159));
    TEST_ASSERT_TRUE(btn.isHeldFor(1000, 6160));
    g_mockPinState = HIGH;
    btn.update(6200);
    btn.update(6260);
    TEST_ASSERT_TRUE(btn.wasReleased());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_button_initial_state);
    RUN_TEST(test_button_press_and_debounce);
    RUN_TEST(test_button_long_press_held);
    RUN_TEST(test_button_release);
    RUN_TEST(test_consumed_menu_entry_press_does_not_trigger_hold_or_release);
    return UNITY_END();
}
