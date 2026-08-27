#include <unity.h>
#include <cstring>
#include "i18n.h"
#include "i18n.cpp"

void setUp(void) {
    i18nSetLanguage(Language::DE);
}

void tearDown(void) {}

void test_i18n_default_language_is_de(void) {
    TEST_ASSERT_EQUAL((int)Language::DE, (int)i18nGetLanguage());
    TEST_ASSERT_EQUAL_STRING("de", i18nGetLanguageCode());
    TEST_ASSERT_EQUAL_STRING("Deutsch", i18nGetLanguageName());
    TEST_ASSERT_EQUAL_STRING("Bereit", i18n().ready);
    TEST_ASSERT_EQUAL_STRING("STANDBY", i18n().prepare);
    TEST_ASSERT_EQUAL_STRING("Stop plate treffen", i18n().drillHint);
}

void test_i18n_switch_to_en(void) {
    i18nSetLanguage(Language::EN);
    TEST_ASSERT_EQUAL((int)Language::EN, (int)i18nGetLanguage());
    TEST_ASSERT_EQUAL_STRING("en", i18nGetLanguageCode());
    TEST_ASSERT_EQUAL_STRING("English", i18nGetLanguageName());
    TEST_ASSERT_EQUAL_STRING("Ready", i18n().ready);
    TEST_ASSERT_EQUAL_STRING("STANDBY", i18n().prepare);
    TEST_ASSERT_EQUAL_STRING("Hit Stop plate", i18n().drillHint);
    TEST_ASSERT_EQUAL_STRING("START", i18nWeb().trainingStart);
    TEST_ASSERT_EQUAL_STRING("Start Countdown", i18nWeb().trainingCountdownStart);
    TEST_ASSERT_EQUAL_STRING("Stage", i18nWeb().trainingStage);
    TEST_ASSERT_EQUAL_STRING("Saved on this phone only", i18nWeb().trainingLocalOnly);
}

void test_i18n_shooting_sports_terminology_preserved(void) {
    // English shooting sports terminology standard check in both languages
    i18nSetLanguage(Language::DE);
    TEST_ASSERT_EQUAL_STRING("Stop plate", i18n().stopPlate);
    TEST_ASSERT_EQUAL_STRING("SHOOT!", i18n().go);
    TEST_ASSERT_EQUAL_STRING("STANDBY", i18n().prepare);
    TEST_ASSERT_EQUAL_STRING("Split: +", i18n().splitPrefix);

    i18nSetLanguage(Language::EN);
    TEST_ASSERT_EQUAL_STRING("Stop plate", i18n().stopPlate);
    TEST_ASSERT_EQUAL_STRING("SHOOT!", i18n().go);
    TEST_ASSERT_EQUAL_STRING("STANDBY", i18n().prepare);
    TEST_ASSERT_EQUAL_STRING("Split: +", i18n().splitPrefix);
}

void test_i18n_standard_stage_names_preserved(void) {
    i18nSetLanguage(Language::DE);
    TEST_ASSERT_EQUAL_STRING("5 to Go", i18nWeb().trainingStage5ToGo);
    TEST_ASSERT_EQUAL_STRING("Showdown", i18nWeb().trainingStageShowdown);
    TEST_ASSERT_EQUAL_STRING("Smoke & Hope", i18nWeb().trainingStageSmokeAndHope);
    TEST_ASSERT_EQUAL_STRING("Outer Limits", i18nWeb().trainingStageOuterLimits);
    TEST_ASSERT_EQUAL_STRING("Accelerator", i18nWeb().trainingStageAccelerator);
    TEST_ASSERT_EQUAL_STRING("The Pendulum", i18nWeb().trainingStageThePendulum);
    TEST_ASSERT_EQUAL_STRING("Speed Option", i18nWeb().trainingStageSpeedOption);
    TEST_ASSERT_EQUAL_STRING("Roundabout", i18nWeb().trainingStageRoundabout);

    i18nSetLanguage(Language::EN);
    TEST_ASSERT_EQUAL_STRING("5 to Go", i18nWeb().trainingStage5ToGo);
    TEST_ASSERT_EQUAL_STRING("Showdown", i18nWeb().trainingStageShowdown);
    TEST_ASSERT_EQUAL_STRING("Smoke & Hope", i18nWeb().trainingStageSmokeAndHope);
    TEST_ASSERT_EQUAL_STRING("Outer Limits", i18nWeb().trainingStageOuterLimits);
    TEST_ASSERT_EQUAL_STRING("Accelerator", i18nWeb().trainingStageAccelerator);
    TEST_ASSERT_EQUAL_STRING("The Pendulum", i18nWeb().trainingStageThePendulum);
    TEST_ASSERT_EQUAL_STRING("Speed Option", i18nWeb().trainingStageSpeedOption);
    TEST_ASSERT_EQUAL_STRING("Roundabout", i18nWeb().trainingStageRoundabout);
}

void test_i18n_display_strings_max_length_de(void) {
    i18nSetLanguage(Language::DE);
    const DisplayStrings& d = i18n();

    // Check key single-line display strings are <= 21 chars
    TEST_ASSERT_TRUE(strlen(d.ready) <= 21);
    TEST_ASSERT_TRUE(strlen(d.readyForStart) <= 21);
    TEST_ASSERT_TRUE(strlen(d.pressStart) <= 21);
    TEST_ASSERT_TRUE(strlen(d.go) <= 21);
    TEST_ASSERT_TRUE(strlen(d.finish) <= 21);
    TEST_ASSERT_TRUE(strlen(d.prepare) <= 21);
    TEST_ASSERT_TRUE(strlen(d.waitForBeep) <= 21);
    TEST_ASSERT_TRUE(strlen(d.timeout) <= 21);
    TEST_ASSERT_TRUE(strlen(d.falseStart) <= 21);
    TEST_ASSERT_TRUE(strlen(d.falseStartHint) <= 21);
    TEST_ASSERT_TRUE(strlen(d.drillHint) <= 21);
    TEST_ASSERT_TRUE(strlen(d.footerFinished) <= 21);
    TEST_ASSERT_TRUE(strlen(d.footerSplit) <= 21);
    TEST_ASSERT_TRUE(strlen(d.footerStandby) <= 21);
    TEST_ASSERT_TRUE(strlen(d.footerStandbyDrill) <= 21);
    TEST_ASSERT_TRUE(strlen(d.footerReaction) <= 21);
    TEST_ASSERT_TRUE(strlen(d.catTargets) <= 21);
    TEST_ASSERT_TRUE(strlen(d.catPhoneTraining) <= 21);
}

void test_i18n_display_strings_max_length_en(void) {
    i18nSetLanguage(Language::EN);
    const DisplayStrings& d = i18n();

    // Check key single-line display strings are <= 21 chars
    TEST_ASSERT_TRUE(strlen(d.ready) <= 21);
    TEST_ASSERT_TRUE(strlen(d.readyForStart) <= 21);
    TEST_ASSERT_TRUE(strlen(d.pressStart) <= 21);
    TEST_ASSERT_TRUE(strlen(d.go) <= 21);
    TEST_ASSERT_TRUE(strlen(d.finish) <= 21);
    TEST_ASSERT_TRUE(strlen(d.prepare) <= 21);
    TEST_ASSERT_TRUE(strlen(d.waitForBeep) <= 21);
    TEST_ASSERT_TRUE(strlen(d.timeout) <= 21);
    TEST_ASSERT_TRUE(strlen(d.falseStart) <= 21);
    TEST_ASSERT_TRUE(strlen(d.falseStartHint) <= 21);
    TEST_ASSERT_TRUE(strlen(d.drillHint) <= 21);
    TEST_ASSERT_TRUE(strlen(d.footerFinished) <= 21);
    TEST_ASSERT_TRUE(strlen(d.footerSplit) <= 21);
    TEST_ASSERT_TRUE(strlen(d.footerStandby) <= 21);
    TEST_ASSERT_TRUE(strlen(d.footerStandbyDrill) <= 21);
    TEST_ASSERT_TRUE(strlen(d.footerReaction) <= 21);
    TEST_ASSERT_TRUE(strlen(d.catTargets) <= 21);
    TEST_ASSERT_TRUE(strlen(d.catPhoneTraining) <= 21);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_i18n_default_language_is_de);
    RUN_TEST(test_i18n_switch_to_en);
    RUN_TEST(test_i18n_shooting_sports_terminology_preserved);
    RUN_TEST(test_i18n_standard_stage_names_preserved);
    RUN_TEST(test_i18n_display_strings_max_length_de);
    RUN_TEST(test_i18n_display_strings_max_length_en);
    return UNITY_END();
}
