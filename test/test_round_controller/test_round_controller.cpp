#include <unity.h>
#include "round_controller.h"
#include "round_result.h"
#include "storage.h"
#include "menu.h"
#include "button.h"
#include "remote_control.h"
#include "Preferences.h"

extern void onSensorHitIdx(int);
extern bool testServerConnected, testCommitAllowed;
extern int testCommitCount, testMissCount, testPenaltyCount;
extern unsigned long testRawTime, testFinalTime;
static Button startButton(14), stopButton(12), resetButton(13);
static HwButtons buttons{startButton, stopButton, resetButton};

static void tick(unsigned long time)
{
  g_mockMillis = time;
  roundControllerTick(time);
}

static void edge(Button& button, bool down)
{
  g_mockPinState = down ? LOW : HIGH;
  button.update(g_mockMillis);
  g_mockMillis += 40;
  button.update(g_mockMillis);
  roundControllerTick(g_mockMillis);
}

static void click(Button& button)
{
  edge(button, true);
  edge(button, false);
}

void setUp()
{
  Preferences::resetAllStores();
  g_mockMillis = 10000;
  g_mockPinState = HIGH;
  startButton = Button(14); stopButton = Button(12); resetButton = Button(13);
  startButton.init(); stopButton.init(); resetButton.init();
  testServerConnected = true;
  testCommitAllowed = true;
  testCommitCount = 0;
  remoteControlInit();
  roundControllerInit(buttons);
}
void tearDown() {}

static unsigned long startRun(int mode = 0)
{
  TEST_ASSERT_TRUE(roundControllerSetRoundMode(mode));
  click(startButton);
  TEST_ASSERT_EQUAL(DELAY, roundControllerGetState());
  tick(g_mockMillis + 5000);
  TEST_ASSERT_EQUAL(RUNNING, roundControllerGetState());
  return g_mockMillis;
}

static void hit(int target, unsigned long time)
{
  g_mockMillis = time;
  onSensorHitIdx(target);
}

void test_competition_freezes_first_stop_and_excludes_later_hits()
{
  unsigned long began = startRun();
  hit(1, began + 500);
  hit(0, began + 1000);
  hit(2, began + 1030);
  hit(0, began + 1100);
  tick(began + 1200);
  TEST_ASSERT_EQUAL(FINISHED, roundControllerGetState());
  TEST_ASSERT_EQUAL(3, roundControllerGetAutomaticMissCount());
  TEST_ASSERT_EQUAL(10000, roundControllerGetElapsedMs(g_mockMillis));
  bool mask[MAX_TARGETS] = {};
  roundControllerGetHitMask(mask, MAX_TARGETS);
  TEST_ASSERT_FALSE(mask[2]);
  int targets[MAX_RECORDED_HITS]; unsigned long splits[MAX_RECORDED_HITS];
  TEST_ASSERT_EQUAL(2, roundControllerGetRecordedHits(targets, splits, MAX_RECORDED_HITS));
  click(resetButton);
  TEST_ASSERT_EQUAL(1, testCommitCount);
  TEST_ASSERT_EQUAL(1000, testRawTime);
  TEST_ASSERT_EQUAL(3, testMissCount);
  TEST_ASSERT_EQUAL(STANDBY, roundControllerGetState());
}

void test_all_targets_requires_a_stop_after_consumed_intermediates()
{
  unsigned long began = startRun(1);
  hit(0, began + 500);
  for (int target = 1; target < MAX_TARGETS; target++) { hit(target, began + 500 + target * 100); }
  tick(began + 1000);
  TEST_ASSERT_EQUAL(RUNNING, roundControllerGetState());
  hit(0, began + 1500);
  tick(began + 1600);
  TEST_ASSERT_EQUAL(FINISHED, roundControllerGetState());
  TEST_ASSERT_EQUAL(1500, roundControllerGetElapsedMs(g_mockMillis));
}

void test_unconfirmed_result_survives_inactivity_and_failed_commit()
{
  unsigned long began = startRun();
  hit(0, began + 1000); tick(began + 1000);
  tick(began + 20UL * 60000UL);
  TEST_ASSERT_EQUAL(FINISHED, roundControllerGetState());
  TEST_ASSERT_TRUE(roundResultIsPending());
  TEST_ASSERT_EQUAL(0, testCommitCount);
  testCommitAllowed = false;
  click(resetButton);
  TEST_ASSERT_TRUE(roundResultIsPending());
  testCommitAllowed = true;
  tick(g_mockMillis + 400);
  click(resetButton);
  TEST_ASSERT_EQUAL(1, testCommitCount);
  TEST_ASSERT_FALSE(roundResultIsPending());
}

void test_late_hits_cannot_create_unconfirmable_raw_time()
{
  unsigned long began = startRun();
  hit(0, began + 30010); tick(began + 30020);
  click(resetButton);
  TEST_ASSERT_EQUAL(30000, testRawTime);
  TEST_ASSERT_EQUAL(30000, testFinalTime);
}

void test_training_penalty_correction_preserves_id_and_updates_score()
{
  testServerConnected = false;
  TEST_ASSERT_TRUE(remoteControlBeginPairing(g_mockMillis));
  char token[REMOTE_SESSION_TOKEN_LENGTH + 1];
  TEST_ASSERT_EQUAL(RemotePairResult::Success,
    remoteControlPair(remoteControlGetPairingPin(), g_mockMillis, token, sizeof(token)));
  unsigned long began = startRun();
  hit(0, began + 1000); tick(began + 1000);
  TrainingResult original, corrected;
  TEST_ASSERT_TRUE(roundControllerGetLatestTrainingResult(original));
  click(stopButton);
  TEST_ASSERT_TRUE(roundControllerGetLatestTrainingResult(corrected));
  TEST_ASSERT_EQUAL_STRING(original.id, corrected.id);
  TEST_ASSERT_EQUAL(16000, corrected.timeMs);
  TEST_ASSERT_EQUAL(5, corrected.penaltyCount);
  TEST_ASSERT_EQUAL(1000, corrected.rawTimeMs);
  TEST_ASSERT_EQUAL(1, corrected.hitCount);
  // Beginning the next attempt must not modify the previous published run.
  click(resetButton); tick(g_mockMillis + 400);
  startRun();
  TEST_ASSERT_TRUE(roundControllerGetLatestTrainingResult(corrected));
  TEST_ASSERT_EQUAL(16000, corrected.timeMs);
}

void test_settings_survive_boot_and_menu_exit_without_edits()
{
  storageSaveRoundMode(2);
  storageSaveScreensaverTimeout(15);
  storageSavePreparationDelay(10);
  roundControllerInit(buttons);
  TEST_ASSERT_EQUAL(2, roundControllerGetRoundMode());
  TEST_ASSERT_EQUAL(15, menuGetScreensaverTimeout());
  TEST_ASSERT_EQUAL(10, roundControllerGetPreparationDelay());
  // Hold the real entry chord, then release it before exiting the main menu.
  edge(stopButton, true); edge(resetButton, true);
  tick(g_mockMillis + 1600);
  TEST_ASSERT_EQUAL(MENU, roundControllerGetState());
  edge(stopButton, false); edge(resetButton, false);
  edge(resetButton, true); tick(g_mockMillis + 1100); edge(resetButton, false);
  TEST_ASSERT_EQUAL(STANDBY, roundControllerGetState());
  TEST_ASSERT_EQUAL(2, storageLoadRoundMode());
  TEST_ASSERT_EQUAL(15, storageLoadScreensaverTimeout());
  roundControllerInit(buttons);
  TEST_ASSERT_EQUAL(2, roundControllerGetRoundMode());
  TEST_ASSERT_EQUAL(15, roundControllerGetScreensaverTimeout());
}

void test_physical_long_reset_discards_without_submission()
{
  unsigned long began = startRun();
  hit(0, began + 1000); tick(began + 1000);
  edge(resetButton, true); tick(g_mockMillis + 1100); edge(resetButton, false);
  TEST_ASSERT_EQUAL(STANDBY, roundControllerGetState());
  TEST_ASSERT_FALSE(roundResultIsPending());
  TEST_ASSERT_EQUAL(0, testCommitCount);
}

void test_all_targets_manual_stop_cannot_remove_automatic_misses()
{
  startRun(1);
  click(stopButton);
  tick(g_mockMillis + 400);
  edge(stopButton, true); tick(g_mockMillis + 1100); edge(stopButton, false);
  TEST_ASSERT_EQUAL(4, roundControllerGetPenaltyCount());
  click(resetButton);
  TEST_ASSERT_EQUAL(1, testCommitCount);
  TEST_ASSERT_EQUAL(4, testMissCount);
  TEST_ASSERT_EQUAL(4, testPenaltyCount);
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_physical_long_reset_discards_without_submission);
  RUN_TEST(test_all_targets_manual_stop_cannot_remove_automatic_misses);
  RUN_TEST(test_competition_freezes_first_stop_and_excludes_later_hits);
  RUN_TEST(test_all_targets_requires_a_stop_after_consumed_intermediates);
  RUN_TEST(test_unconfirmed_result_survives_inactivity_and_failed_commit);
  RUN_TEST(test_late_hits_cannot_create_unconfirmable_raw_time);
  RUN_TEST(test_training_penalty_correction_preserves_id_and_updates_score);
  RUN_TEST(test_settings_survive_boot_and_menu_exit_without_edits);
  return UNITY_END();
}
