#include <unity.h>
#include <cstring>
#include <cstdio>
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
    TEST_ASSERT_EQUAL_STRING("Ready", i18n().ready);
    TEST_ASSERT_EQUAL_STRING("STANDBY", i18n().prepare);
    TEST_ASSERT_EQUAL_STRING("Stop plate treffen", i18n().drillHint);
    TEST_ASSERT_EQUAL_STRING("Vorlaufzeit", i18n().prepDelay);
    TEST_ASSERT_EQUAL_STRING("Vorlaufzeit (0-30s)", i18nWeb().webPrepDelay);
    TEST_ASSERT_EQUAL_STRING("Abmelden", i18nWeb().trainingDisconnect);
}

void test_i18n_switch_to_en(void) {
    i18nSetLanguage(Language::EN);
    TEST_ASSERT_EQUAL((int)Language::EN, (int)i18nGetLanguage());
    TEST_ASSERT_EQUAL_STRING("en", i18nGetLanguageCode());
    TEST_ASSERT_EQUAL_STRING("English", i18nGetLanguageName());
    TEST_ASSERT_EQUAL_STRING("Ready", i18n().ready);
    TEST_ASSERT_EQUAL_STRING("STANDBY", i18n().prepare);
    TEST_ASSERT_EQUAL_STRING("Hit Stop plate", i18n().drillHint);
    TEST_ASSERT_EQUAL_STRING("Prep Delay", i18n().prepDelay);
    TEST_ASSERT_EQUAL_STRING("Prep Delay (0-30s)", i18nWeb().webPrepDelay);
    TEST_ASSERT_EQUAL_STRING("START", i18nWeb().trainingStart);
    TEST_ASSERT_EQUAL_STRING("Start Countdown", i18nWeb().trainingCountdownStart);
    TEST_ASSERT_EQUAL_STRING("Stage", i18nWeb().trainingStage);
    TEST_ASSERT_EQUAL_STRING("Saved on this phone only", i18nWeb().trainingLocalOnly);
    TEST_ASSERT_EQUAL_STRING("Disconnect", i18nWeb().trainingDisconnect);
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
    TEST_ASSERT_TRUE(strlen(d.prepDelay) <= 21);
    TEST_ASSERT_TRUE(strlen(d.titlePrepDelay) <= 21);
    TEST_ASSERT_TRUE(strlen(d.paramPrepDelay) <= 21);
    TEST_ASSERT_TRUE(strlen(d.hintPrepDelay) <= 21);
    TEST_ASSERT_TRUE(strlen(d.testClearOutbox) <= 21);
    TEST_ASSERT_TRUE(strlen(d.confirmClearOutboxTitle) <= 21);
    TEST_ASSERT_TRUE(strlen(d.confirmClearOutboxLine2) <= 21);
    char clearMsg[32];
    snprintf(clearMsg, sizeof(clearMsg), d.confirmClearOutboxLine1, 20U);
    TEST_ASSERT_TRUE(strlen(clearMsg) <= 21);
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
    TEST_ASSERT_TRUE(strlen(d.prepDelay) <= 21);
    TEST_ASSERT_TRUE(strlen(d.titlePrepDelay) <= 21);
    TEST_ASSERT_TRUE(strlen(d.paramPrepDelay) <= 21);
    TEST_ASSERT_TRUE(strlen(d.hintPrepDelay) <= 21);
    TEST_ASSERT_TRUE(strlen(d.testClearOutbox) <= 21);
    TEST_ASSERT_TRUE(strlen(d.confirmClearOutboxTitle) <= 21);
    TEST_ASSERT_TRUE(strlen(d.confirmClearOutboxLine2) <= 21);
    char clearMsg[32];
    snprintf(clearMsg, sizeof(clearMsg), d.confirmClearOutboxLine1, 20U);
    TEST_ASSERT_TRUE(strlen(clearMsg) <= 21);
}

void test_outbox_messages_fit_the_display(void) {
    const Language languages[] = {Language::DE, Language::EN};
    for (Language language : languages) {
        i18nSetLanguage(language);
        const DisplayStrings& d = i18n();
        TEST_ASSERT_TRUE(strlen(d.outboxFull) <= 21);
        TEST_ASSERT_TRUE(strlen(d.outboxStorageError) <= 21);
        TEST_ASSERT_TRUE(strlen(d.outboxBlocked) <= 20);
        TEST_ASSERT_TRUE(strlen(d.outboxKeepResult) <= 20);
        TEST_ASSERT_TRUE(strlen(d.outboxKeepPower) <= 20);
        TEST_ASSERT_TRUE(strlen(d.outboxConnect) <= 21);
        TEST_ASSERT_TRUE(strlen(d.outboxService) <= 20);
        char pending[32];
        snprintf(pending, sizeof(pending), d.outboxPendingFormat, 20U, 20U);
        TEST_ASSERT_TRUE(strlen(pending) <= 20);
    }
}

void test_server_ipv4_address_fits_menu_without_truncation(void) {
    const Language languages[] = {Language::DE, Language::EN};
    for (Language language : languages) {
        i18nSetLanguage(language);
        char line[24];
        snprintf(line, sizeof(line), i18n().networkServerFormat, "192.168.178.123");
        TEST_ASSERT_EQUAL_STRING("Srv:192.168.178.123", line);
        snprintf(line, sizeof(line), i18n().networkServerFormat, "255.255.255.255");
        TEST_ASSERT_EQUAL_STRING("Srv:255.255.255.255", line);
        TEST_ASSERT_TRUE(strlen(line) <= 19);
    }
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_i18n_default_language_is_de);
    RUN_TEST(test_i18n_switch_to_en);
    RUN_TEST(test_i18n_shooting_sports_terminology_preserved);
    RUN_TEST(test_i18n_standard_stage_names_preserved);
    RUN_TEST(test_i18n_display_strings_max_length_de);
    RUN_TEST(test_i18n_display_strings_max_length_en);
    RUN_TEST(test_outbox_messages_fit_the_display);
    RUN_TEST(test_server_ipv4_address_fits_menu_without_truncation);
    return UNITY_END();
}
