#include <unity.h>
#include "storage.h"
#include "storage.cpp"
#include "Preferences.h"
#include "Arduino.h"
#include "mock_globals.cpp"

void setUp(void) {
    Preferences::resetAllStores();
}

void tearDown(void) {}

void test_storage_wifi_save_and_load(void) {
    String ssid, pass;
    TEST_ASSERT_FALSE(storageLoadWiFi(ssid, pass));

    storageSaveWiFi("MyHomeNet", "SecretPass123");

    String loadedSsid, loadedPass;
    TEST_ASSERT_TRUE(storageLoadWiFi(loadedSsid, loadedPass));
    TEST_ASSERT_EQUAL_STRING("MyHomeNet", loadedSsid.c_str());
    TEST_ASSERT_EQUAL_STRING("SecretPass123", loadedPass.c_str());
}

void test_storage_wifi_clear(void) {
    storageSaveWiFi("TempWiFi", "TempPass");
    storageClearWiFi();

    String loadedSsid, loadedPass;
    TEST_ASSERT_FALSE(storageLoadWiFi(loadedSsid, loadedPass));
}

void test_storage_device_fallback_name(void) {
    String name, ip;
    uint16_t port;
    storageLoadDevice(name, ip, port, "DefaultTarget");

    TEST_ASSERT_EQUAL_STRING("DefaultTarget", name.c_str());
    TEST_ASSERT_EQUAL_STRING("", ip.c_str());
    TEST_ASSERT_EQUAL(53421, port); // Default port from config.h
}

void test_storage_device_save_and_load(void) {
    storageSaveDevice("Target_01", "192.168.1.100", 53421);

    String name, ip;
    uint16_t port = 0;
    storageLoadDevice(name, ip, port, "Fallback");

    TEST_ASSERT_EQUAL_STRING("Target_01", name.c_str());
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", ip.c_str());
    TEST_ASSERT_EQUAL(53421, port);
}

void test_storage_target_count(void) {
    TEST_ASSERT_EQUAL(5, storageLoadTargetCount()); // Default MAX_TARGETS

    storageSaveTargetCount(4);
    TEST_ASSERT_EQUAL(4, storageLoadTargetCount());
}

void test_storage_legacy_round_mode_migrates_without_overriding_reaction_drill()
{
    Preferences legacy;
    legacy.begin("easysteel");
    legacy.putBool("require_all", true);
    legacy.end();
    TEST_ASSERT_EQUAL(1, storageLoadRoundMode());
    storageSaveRoundMode(2);
    TEST_ASSERT_EQUAL(2, storageLoadRoundMode());
}

void test_storage_screensaver_timeout(void) {
    TEST_ASSERT_EQUAL(5, storageLoadScreensaverTimeout()); // Default 5 minutes

    storageSaveScreensaverTimeout(15);
    TEST_ASSERT_EQUAL(15, storageLoadScreensaverTimeout());

    storageSaveScreensaverTimeout(0); // Disabled
    TEST_ASSERT_EQUAL(0, storageLoadScreensaverTimeout());
}

void test_storage_language(void) {
    TEST_ASSERT_EQUAL(0, storageLoadLanguage()); // Default 0 (DE)

    storageSaveLanguage(1); // EN
    TEST_ASSERT_EQUAL(1, storageLoadLanguage());

    storageSaveLanguage(0); // DE
    TEST_ASSERT_EQUAL(0, storageLoadLanguage());
}

void test_storage_round_mode(void) {

    TEST_ASSERT_EQUAL(0, storageLoadRoundMode()); // Default 0 (Wettkampf)

    storageSaveRoundMode(1); // Alle Ziele
    TEST_ASSERT_EQUAL(1, storageLoadRoundMode());

    storageSaveRoundMode(2); // Reaktions-Drill
    TEST_ASSERT_EQUAL(2, storageLoadRoundMode());
}

void test_storage_preparation_delay(void) {
    TEST_ASSERT_EQUAL(0, storageLoadPreparationDelay()); // Default 0 (disabled)

    storageSavePreparationDelay(10);
    TEST_ASSERT_EQUAL(10, storageLoadPreparationDelay());

    storageSavePreparationDelay(30);
    TEST_ASSERT_EQUAL(30, storageLoadPreparationDelay());

    storageSavePreparationDelay(0);
    TEST_ASSERT_EQUAL(0, storageLoadPreparationDelay());

    storageSavePreparationDelay(-5); // clamped to 0
    TEST_ASSERT_EQUAL(0, storageLoadPreparationDelay());

    storageSavePreparationDelay(50); // clamped to 30
    TEST_ASSERT_EQUAL(30, storageLoadPreparationDelay());
}

static uint32_t passwordRandom() { return 0x1234abcd; }

void test_device_secret_is_persisted_and_survives_configuration_reset()
{
    String password;
    TEST_ASSERT_TRUE(storageLoadDevicePassword(password, passwordRandom));
    TEST_ASSERT_EQUAL_STRING("1234abcd1234abcd", password.c_str());
    storageClearDevice();
    storageClearWiFi();
    String loaded;
    TEST_ASSERT_TRUE(storageLoadDevicePassword(loaded, nullptr));
    TEST_ASSERT_EQUAL_STRING(password.c_str(), loaded.c_str());
}

void test_device_secret_storage_failure_never_returns_a_temporary_password()
{
    String password("stale");
    Preferences::failOpen = true;
    TEST_ASSERT_FALSE(storageLoadDevicePassword(password, passwordRandom));
    TEST_ASSERT_EQUAL(0, password.length());
    Preferences::failOpen = false;
    Preferences::failWrite = true;
    TEST_ASSERT_FALSE(storageLoadDevicePassword(password, passwordRandom));
    TEST_ASSERT_EQUAL(0, password.length());
    Preferences::failWrite = false;
    Preferences corrupt;
    corrupt.begin("security");
    corrupt.putString("password", "not-a-valid-pass!");
    corrupt.end();
    TEST_ASSERT_FALSE(storageLoadDevicePassword(password, passwordRandom));
    TEST_ASSERT_EQUAL(0, password.length());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_device_secret_is_persisted_and_survives_configuration_reset);
    RUN_TEST(test_device_secret_storage_failure_never_returns_a_temporary_password);
    RUN_TEST(test_storage_wifi_save_and_load);
    RUN_TEST(test_storage_wifi_clear);
    RUN_TEST(test_storage_device_fallback_name);
    RUN_TEST(test_storage_device_save_and_load);
    RUN_TEST(test_storage_target_count);
    RUN_TEST(test_storage_legacy_round_mode_migrates_without_overriding_reaction_drill);
    RUN_TEST(test_storage_round_mode);
    RUN_TEST(test_storage_screensaver_timeout);
    RUN_TEST(test_storage_language);
    RUN_TEST(test_storage_preparation_delay);
    return UNITY_END();
}
