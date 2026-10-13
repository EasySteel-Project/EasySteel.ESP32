#include "display.h"
#include "network.h"
#include "offline_buffer.h"
#include "mock_globals.cpp"
bool displayInit() { return true; }
void displayUpdate(const char *line1, const char *line2) {}
void displayUpdateBigTime(const char *label, const char *numStr, const char *suffix, float progress, int penaltyCount) {}
void displayUpdateStandby(const char *deviceName, const char *stageName, unsigned long lastTimeMs, int penaltyCount, bool serverConnected, int targetCount, int roundMode, const char *outboxStatus) {}
void displayUpdateRunning(const char *timeStr, float progress, const bool hitMask[5], int targetCount) {}
void displayUpdateFinished(const char *timeStr, int penaltyCount, const bool hitMask[5], int targetCount, const char *footer) {}
void displayUpdateSplitDetail(int hitIndex, int totalHits, int targetId, const char *splitTimeStr, const char *deltaStr, int penaltyCount) {}
void displayUpdateReaction(const char *timeStr, int hitTarget) {}
void displayShowStartupAnimation() {}
void displayUpdateMenuList(const char *headerTitle, const char *const *items, int itemCount, int selectedIndex, const char *footer) {}
void displayUpdateValueEdit(const char *categoryTitle, const char *paramName, const char *valueStr, const char *hint, const char *footer) {}
void displayUpdateConfirmation(const char *title, const char *line1, const char *line2, const char *footer) {}
void displayUpdateSensorMatrix(const bool hitTargets[5], int lastHitIndex) {}
void displayUpdateDeviceInfo(const char *fwVersion, const char *deviceName, const char *ip, const char *mac, unsigned long uptimeSec) {}
void displayScreensaverTick(unsigned long currentMillis, const char *deviceName, bool isConnected) {}
void displayExitScreensaver() {}

bool testServerConnected = false;
bool testCommitAllowed = true;
int testCommitCount = 0;
unsigned long testRawTime = 0;
unsigned long testFinalTime = 0;
int testMissCount = 0;
int testPenaltyCount = 0;
bool networkIsProvisioning() { return false; }
bool networkIsWiFiConnected() { return true; }
bool networkIsServerConnected() { return testServerConnected; }
void networkSendEvent(const char*, int, unsigned long) {}
bool networkSendResult(unsigned long raw, unsigned long time, int misses, int penalties, const char*)
{
  if (!testCommitAllowed) { return false; }
  testCommitCount++; testRawTime = raw; testFinalTime = time; testMissCount = misses; testPenaltyCount = penalties;
  return true;
}
static String testName("Test");
const String& networkGetDeviceName() { return testName; }
const String& networkGetDeviceID() { return testName; }
const String& networkGetDevicePassword() { return testName; }
const String& networkGetServerIP() { return testName; }
String networkGetSSID() { return "Test"; }
String networkGetLocalIP() { return "127.0.0.1"; }
uint16_t networkGetTCPPort() { return 53421; }
bool networkReconnectWiFi(Button*) { return false; }
void networkStartAPMode() {}
void networkStartWPS() {}
void networkResetCredentials() {}
bool offlineBufferCanAccept() { return true; }
bool offlineBufferHasStorageError() { return !testCommitAllowed; }
size_t offlineBufferGetCount() { return 0; }
bool offlineBufferClear() { return true; }
