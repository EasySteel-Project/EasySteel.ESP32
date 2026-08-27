#include "menu.h"
#include "round_controller.h"
#include "button.h"
#include "buzzer.h"
#include "display.h"
#include "network.h"
#include "storage.h"
#include "ui_strings.h"
#include "leds.h"
#include "sensor_isr.h"
#include "remote_control.h"
#include "training_result.h"
#include <WiFi.h>

static Button* btnStart = nullptr;
static Button* btnStop = nullptr;
static Button* btnReset = nullptr;

enum MenuState
{
  MENU_STATE_MAIN_CAT,
  MENU_STATE_TARGET_MENU,
  MENU_STATE_TARGET_COUNT_EDIT,
  MENU_STATE_STOP_MODE_EDIT,
  MENU_STATE_SCREENSAVER_EDIT,
  MENU_STATE_LANGUAGE_EDIT,
  MENU_STATE_NET_MENU,
  MENU_STATE_REMOTE_PAIRING,
  MENU_STATE_WIFI_RESET_CONFIRM,
  MENU_STATE_TEST_MENU,
  MENU_STATE_TEST_SENSORS,
  MENU_STATE_TEST_LEDS_BUZZER,
  MENU_STATE_INFO
};

static bool menuActive = false;
static MenuState currentState = MENU_STATE_MAIN_CAT;

// Selection indices
static int mainCatIndex = 0;       // 0..5
static int targetSubIndex = 0;     // 0..4
static int netItemIndex = 0;       // 0..11
static int testSelectIndex = 0;    // 0..2

static int configuredTargetCount = MAX_TARGETS;
static bool configuredRequireAllTargets = false;
static int configuredRoundMode = 0;
static int configuredScreensaverTimeout = 5;
static Language configuredLanguage = Language::DE;


// Test execution variables
static unsigned long testTimer = 0;
static int testStep = 0;
static int lastHitTargetIndex = -1;
static bool sensorMatrixHits[MAX_TARGETS] = {false};
static unsigned long resetPressStartTime = 0;
static bool resetWasHeldLong = false;

static constexpr unsigned long HOLD_LONG_PRESS_MS = 1000;
static constexpr int MAIN_CAT_COUNT = 6;
static constexpr int TARGET_SUB_COUNT = 5;
static constexpr int NET_ITEM_COUNT = 12;
static constexpr int TEST_SELECT_COUNT = 3;


void menuInit(HwButtons& buttons)
{
  btnStart = &buttons.start;
  btnStop = &buttons.stop;
  btnReset = &buttons.reset;
}

void menuEnter(unsigned long currentMillis)
{
  menuActive = true;
  currentState = MENU_STATE_MAIN_CAT;
  mainCatIndex = 0;
  targetSubIndex = 0;
  netItemIndex = 0;
  testSelectIndex = 0;
  buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
}

int menuGetTargetCount()
{
  return configuredTargetCount;
}

void menuSetTargetCount(int count)
{
  configuredTargetCount = count;
}

bool menuGetRequireAllTargets()
{
  return configuredRoundMode == 1;
}

void menuSetRequireAllTargets(bool requireAll)
{
  configuredRequireAllTargets = requireAll;
  configuredRoundMode = requireAll ? 1 : 0;
}

int menuGetRoundMode()
{
  return configuredRoundMode;
}

void menuSetRoundMode(int mode)
{
  configuredRoundMode = mode;
  configuredRequireAllTargets = (mode == 1);
}


int menuGetScreensaverTimeout()
{
  return configuredScreensaverTimeout;
}

void menuSetScreensaverTimeout(int minutes)
{
  configuredScreensaverTimeout = minutes;
}

Language menuGetLanguage()
{
  return configuredLanguage;
}

void menuSetLanguage(Language lang)
{
  configuredLanguage = lang;
  i18nSetLanguage(lang);
}



static void stopRunningTest()
{
  ledsReset();
  sensorIsrSetTestMode(false);
  sensorIsrDisable();
  sensorIsrSetRunning(false);
  lastHitTargetIndex = -1;
  for (int i = 0; i < MAX_TARGETS; i++)
  {
    sensorMatrixHits[i] = false;
  }
}

static void menuUpdateDisplay()
{
  switch (currentState)
  {
  case MENU_STATE_MAIN_CAT:
  {
    const char *mainItems[MAIN_CAT_COUNT] = {
      STR_CAT_TARGETS,
      STR_CAT_PHONE_TRAINING,
      STR_CAT_NETWORK,
      STR_CAT_TEST,
      STR_CAT_INFO,
      STR_CAT_EXIT
    };
    char title[24];
    snprintf(title, sizeof(title), STR_MENU_TITLE_MAIN, mainCatIndex + 1, MAIN_CAT_COUNT);
    displayUpdateMenuList(title, mainItems, MAIN_CAT_COUNT, mainCatIndex, STR_FOOTER_NAV);
    break;
  }

  case MENU_STATE_TARGET_MENU:
  {
    char item1[24];
    snprintf(item1, sizeof(item1), "1. %s", STR_TARGET_COUNT);
    char item2[24];
    snprintf(item2, sizeof(item2), "2. %s", STR_STOP_MODE);
    char item3[24];
    snprintf(item3, sizeof(item3), "3. %s", STR_SCREENSAVER);
    char item4[24];
    snprintf(item4, sizeof(item4), "4. %s", STR_LANGUAGE);
    const char *targetItems[TARGET_SUB_COUNT] = {
      item1,
      item2,
      item3,
      item4,
      STR_BACK
    };
    char title[24];
    snprintf(title, sizeof(title), STR_MENU_TITLE_TARGETS, targetSubIndex + 1, TARGET_SUB_COUNT);
    displayUpdateMenuList(title, targetItems, TARGET_SUB_COUNT, targetSubIndex, STR_FOOTER_NAV);
    break;
  }

  case MENU_STATE_TARGET_COUNT_EDIT:
  {
    char valStr[16];
    snprintf(valStr, sizeof(valStr), STR_VALUE_INT_FORMAT, configuredTargetCount);
    displayUpdateValueEdit(STR_TITLE_TARGET_COUNT, STR_PARAM_TARGET_COUNT, valStr, nullptr, STR_FOOTER_EDIT);
    break;
  }

  case MENU_STATE_STOP_MODE_EDIT:
  {
    const char *modeStr = (configuredRoundMode == 2) ? STR_MODE_REACTION : ((configuredRoundMode == 1) ? STR_MODE_ALL_TARGETS : STR_MODE_COMPETITION);
    displayUpdateValueEdit(STR_TITLE_GAME_MODE, STR_PARAM_MODE, modeStr, STR_HINT_GAME_MODE, STR_FOOTER_NAV);
    break;
  }


  case MENU_STATE_SCREENSAVER_EDIT:
  {
    char valStr[16];
    if (configuredScreensaverTimeout == 0)
    {
      snprintf(valStr, sizeof(valStr), STR_VALUE_TEXT_FORMAT, STR_TIMEOUT_OFF);
    }
    else
    {
      snprintf(valStr, sizeof(valStr), STR_VALUE_MINUTES_FORMAT, configuredScreensaverTimeout);
    }
    displayUpdateValueEdit(STR_TITLE_SCREENSAVER, STR_PARAM_SCREENSAVER, valStr, STR_HINT_SCREENSAVER, STR_FOOTER_NAV);
    break;
  }

  case MENU_STATE_LANGUAGE_EDIT:
  {
    const char *langStr = (configuredLanguage == Language::EN) ? STR_LANG_EN : STR_LANG_DE;
    char valStr[16];
    snprintf(valStr, sizeof(valStr), STR_VALUE_TEXT_FORMAT, langStr);
    displayUpdateValueEdit(STR_TITLE_LANGUAGE, STR_PARAM_LANGUAGE, valStr, "DE / EN", STR_FOOTER_NAV);
    break;
  }


  case MENU_STATE_NET_MENU:
  {
    char lineStatus[24];
    if (networkIsProvisioning())
    {
      snprintf(lineStatus, sizeof(lineStatus), STR_NETWORK_STATUS_FORMAT, STR_STATUS_AP);
    }
    else if (networkIsWiFiConnected())
    {
      snprintf(lineStatus, sizeof(lineStatus), STR_NETWORK_STATUS_FORMAT, STR_STATUS_CONNECTED);
    }
    else
    {
      snprintf(lineStatus, sizeof(lineStatus), STR_NETWORK_STATUS_FORMAT, STR_STATUS_DISCONNECTED);
    }

    char lineSsid[24];
    String ssid = networkGetSSID();
    snprintf(lineSsid, sizeof(lineSsid), STR_NETWORK_SSID_FORMAT, ssid.c_str());

    char lineIp[24];
    String ip = networkGetLocalIP();
    snprintf(lineIp, sizeof(lineIp), STR_NETWORK_IP_FORMAT, ip.c_str());

    char lineServer[24];
    String serverIp = networkGetServerIP();
    snprintf(lineServer, sizeof(lineServer), STR_NETWORK_SERVER_FORMAT, serverIp.c_str());

    char linePort[24];
    uint16_t port = networkGetTCPPort();
    snprintf(linePort, sizeof(linePort), STR_NETWORK_PORT_FORMAT, port);

    char lineName[24];
    String name = networkGetDeviceName();
    snprintf(lineName, sizeof(lineName), STR_NETWORK_NAME_FORMAT, name.c_str());

    char lineId[24];
    String id = networkGetDeviceID();
    snprintf(lineId, sizeof(lineId), STR_NETWORK_ID_FORMAT, id.c_str());

    char lineConnect[24];
    snprintf(lineConnect, sizeof(lineConnect), STR_MENU_ACTION_FORMAT, STR_ACTION_WIFI_CONNECT);

    char lineAp[24];
    snprintf(lineAp, sizeof(lineAp), STR_MENU_ACTION_FORMAT, STR_ACTION_AP_MODE);

    char lineWps[24];
    snprintf(lineWps, sizeof(lineWps), STR_MENU_ACTION_FORMAT, STR_ACTION_WPS);

    char lineReset[24];
    snprintf(lineReset, sizeof(lineReset), STR_MENU_ACTION_FORMAT, STR_ACTION_WIFI_RESET);

    const char *netItems[NET_ITEM_COUNT] = {
      lineStatus,
      lineSsid,
      lineIp,
      lineServer,
      linePort,
      lineName,
      lineId,
      lineConnect,
      lineAp,
      lineWps,
      lineReset,
      STR_BACK
    };

    char title[24];
    snprintf(title, sizeof(title), STR_MENU_TITLE_NETWORK, netItemIndex + 1, NET_ITEM_COUNT);
    const char *footer = (netItemIndex >= 7 && netItemIndex <= 10) ? STR_FOOTER_NAV_RUN : STR_FOOTER_NAV;
    displayUpdateMenuList(title, netItems, NET_ITEM_COUNT, netItemIndex, footer);
    break;
  }

  case MENU_STATE_REMOTE_PAIRING:
  {
    char pinValue[16];
    snprintf(pinValue, sizeof(pinValue), STR_VALUE_TEXT_FORMAT, remoteControlGetPairingPin());
    displayUpdateValueEdit(
      STR_REMOTE_PAIRING_TITLE,
      STR_REMOTE_PAIRING_PIN,
      pinValue,
      STR_REMOTE_PAIRING_HINT,
      STR_FOOTER_BACK);
    break;
  }

  case MENU_STATE_WIFI_RESET_CONFIRM:
    displayUpdateConfirmation(STR_CONFIRM_WIFI_TITLE, STR_CONFIRM_WIFI_LINE1, STR_CONFIRM_WIFI_LINE2, STR_FOOTER_CONFIRM);
    break;

  case MENU_STATE_TEST_MENU:
  {
    char test1[24];
    snprintf(test1, sizeof(test1), "1. %s", STR_TEST_SENSORS);
    char test2[24];
    snprintf(test2, sizeof(test2), "2. %s", STR_TEST_LEDS_BUZZER);
    const char *testItems[TEST_SELECT_COUNT] = {
      test1,
      test2,
      STR_BACK
    };
    char title[24];
    snprintf(title, sizeof(title), STR_MENU_TITLE_TESTS, testSelectIndex + 1, TEST_SELECT_COUNT);
    displayUpdateMenuList(title, testItems, TEST_SELECT_COUNT, testSelectIndex, STR_FOOTER_NAV);
    break;
  }

  case MENU_STATE_TEST_SENSORS:
    displayUpdateSensorMatrix(sensorMatrixHits, lastHitTargetIndex);
    break;

  case MENU_STATE_TEST_LEDS_BUZZER:
    displayUpdateValueEdit(STR_TITLE_LED_BUZZER, STR_PARAM_LED_BUZZER, STR_HINT_LED_BUZZER, nullptr, STR_FOOTER_BACK);
    break;

  case MENU_STATE_INFO:
  {
    unsigned long uptimeSec = millis() / 1000;
    displayUpdateDeviceInfo(STR_FIRMWARE_VER, networkGetDeviceName().c_str(), networkGetLocalIP().c_str(), WiFi.macAddress().c_str(), uptimeSec);
    break;
  }
  }
}

bool menuTick(unsigned long currentMillis)
{
  if (!menuActive)
    return false;

  bool startPressed = btnStart->wasPressed();
  bool stopPressed = btnStop->wasPressed();

  // Handle RESET button long press vs short press on release
  bool resetShortPress = false;
  bool resetLongPress = false;

  if (btnReset->wasPressed())
  {
    resetPressStartTime = currentMillis;
    resetWasHeldLong = false;
  }

  if (btnReset->isHeldFor(HOLD_LONG_PRESS_MS, currentMillis) && !resetWasHeldLong)
  {
    resetWasHeldLong = true;
    resetLongPress = true;
    buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
  }

  if (btnReset->wasReleased())
  {
    if (!resetWasHeldLong)
    {
      resetShortPress = true;
    }
  }

  bool displayNeedsUpdate = false;

  if (currentState == MENU_STATE_REMOTE_PAIRING)
  {
    RemoteControlState remoteState = remoteControlGetState();
    if (remoteState == RemoteControlState::Paired)
    {
      buzzerPlay(BuzzerPattern::Confirm, currentMillis);
      menuActive = false;
      return false;
    }
    if (remoteState != RemoteControlState::Pairing)
    {
      currentState = MENU_STATE_MAIN_CAT;
      displayNeedsUpdate = true;
    }
  }

  // --- Long Press Navigation (Back / Parent Menu / Exit) ---
  if (resetLongPress)
  {
    if (currentState == MENU_STATE_TEST_SENSORS || currentState == MENU_STATE_TEST_LEDS_BUZZER)
    {
      stopRunningTest();
      currentState = MENU_STATE_TEST_MENU;
      displayNeedsUpdate = true;
    }
    else if (currentState == MENU_STATE_TARGET_COUNT_EDIT ||
             currentState == MENU_STATE_STOP_MODE_EDIT ||
             currentState == MENU_STATE_SCREENSAVER_EDIT ||
             currentState == MENU_STATE_LANGUAGE_EDIT)
    {
      currentState = MENU_STATE_TARGET_MENU;
      displayNeedsUpdate = true;
    }
    else if (currentState == MENU_STATE_REMOTE_PAIRING)
    {
      remoteControlDisable();
      currentState = MENU_STATE_MAIN_CAT;
      displayNeedsUpdate = true;
    }
    else if (currentState == MENU_STATE_TARGET_MENU ||
             currentState == MENU_STATE_NET_MENU ||
             currentState == MENU_STATE_TEST_MENU ||
             currentState == MENU_STATE_INFO)
    {
      currentState = MENU_STATE_MAIN_CAT;
      displayNeedsUpdate = true;
    }
    else if (currentState == MENU_STATE_WIFI_RESET_CONFIRM)
    {
      currentState = MENU_STATE_NET_MENU;
      displayNeedsUpdate = true;
    }
    else if (currentState == MENU_STATE_MAIN_CAT)
    {
      // Exit menu and persist all settings
      storageSaveTargetCount(configuredTargetCount);
      storageSaveRoundMode(configuredRoundMode);
      storageSaveRequireAllTargets(configuredRoundMode == 1);
      storageSaveScreensaverTimeout(configuredScreensaverTimeout);
      storageSaveLanguage((int)configuredLanguage);
      roundControllerSetScreensaverTimeout(configuredScreensaverTimeout);
      buzzerPlay(BuzzerPattern::Start, currentMillis);
      menuActive = false;
      networkSendEvent("settings_changed", -1, 0);
      return false;
    }
  }

  // --- State Machine ---
  if (!resetLongPress)
  {
    switch (currentState)
    {
    case MENU_STATE_MAIN_CAT:
      if (startPressed)
      {
        mainCatIndex = (mainCatIndex - 1 + MAIN_CAT_COUNT) % MAIN_CAT_COUNT;
        buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
        displayNeedsUpdate = true;
      }
      if (stopPressed)
      {
        mainCatIndex = (mainCatIndex + 1) % MAIN_CAT_COUNT;
        buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
        displayNeedsUpdate = true;
      }
      if (resetShortPress)
      {
        buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
        switch (mainCatIndex)
        {
        case 0: currentState = MENU_STATE_TARGET_MENU; targetSubIndex = 0; break;
        case 1:
          if (remoteControlGetState() == RemoteControlState::Paired)
          {
            remoteControlDisable();
          }
          else if (networkIsWiFiConnected() && remoteControlBeginPairing(currentMillis))
          {
            trainingResultReset();
            buzzerPlay(BuzzerPattern::Start, currentMillis);
            currentState = MENU_STATE_REMOTE_PAIRING;
          }
          else
          {
            buzzerPlay(BuzzerPattern::DoubleShort, currentMillis);
          }
          break;
        case 2: currentState = MENU_STATE_NET_MENU; netItemIndex = 0; break;
        case 3: currentState = MENU_STATE_TEST_MENU; testSelectIndex = 0; break;
        case 4: currentState = MENU_STATE_INFO; break;
        case 5:
          // Exit menu and persist all settings
          storageSaveTargetCount(configuredTargetCount);
          storageSaveRoundMode(configuredRoundMode);
          storageSaveRequireAllTargets(configuredRoundMode == 1);
          storageSaveScreensaverTimeout(configuredScreensaverTimeout);
          storageSaveLanguage((int)configuredLanguage);
          roundControllerSetScreensaverTimeout(configuredScreensaverTimeout);
          buzzerPlay(BuzzerPattern::Start, currentMillis);
          menuActive = false;
          networkSendEvent("settings_changed", -1, 0);
          return false;
        }
        displayNeedsUpdate = true;
      }
      break;

    case MENU_STATE_TARGET_MENU:
      if (startPressed)
      {
        targetSubIndex = (targetSubIndex - 1 + TARGET_SUB_COUNT) % TARGET_SUB_COUNT;
        buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
        displayNeedsUpdate = true;
      }
      if (stopPressed)
      {
        targetSubIndex = (targetSubIndex + 1) % TARGET_SUB_COUNT;
        buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
        displayNeedsUpdate = true;
      }
      if (resetShortPress)
      {
        buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
        switch (targetSubIndex)
        {
        case 0: currentState = MENU_STATE_TARGET_COUNT_EDIT; break;
        case 1: currentState = MENU_STATE_STOP_MODE_EDIT; break;
        case 2: currentState = MENU_STATE_SCREENSAVER_EDIT; break;
        case 3: currentState = MENU_STATE_LANGUAGE_EDIT; break;
        case 4: currentState = MENU_STATE_MAIN_CAT; break;
        }
        displayNeedsUpdate = true;
      }
      break;

    case MENU_STATE_TARGET_COUNT_EDIT:
      if (startPressed && configuredTargetCount < MAX_TARGETS)
      {
        configuredTargetCount++;
        buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
        displayNeedsUpdate = true;
      }
      if (stopPressed && configuredTargetCount > 1)
      {
        configuredTargetCount--;
        buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
        displayNeedsUpdate = true;
      }
      if (resetShortPress)
      {
        storageSaveTargetCount(configuredTargetCount);
        buzzerPlay(BuzzerPattern::Start, currentMillis);
        currentState = MENU_STATE_TARGET_MENU;
        displayNeedsUpdate = true;
      }
      break;

    case MENU_STATE_STOP_MODE_EDIT:
      if (startPressed)
      {
        configuredRoundMode = (configuredRoundMode + 1) % 3;
        configuredRequireAllTargets = (configuredRoundMode == 1);
        buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
        displayNeedsUpdate = true;
      }
      if (stopPressed)
      {
        configuredRoundMode = (configuredRoundMode - 1 + 3) % 3;
        configuredRequireAllTargets = (configuredRoundMode == 1);
        buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
        displayNeedsUpdate = true;
      }
      if (resetShortPress)
      {
        storageSaveRoundMode(configuredRoundMode);
        storageSaveRequireAllTargets(configuredRoundMode == 1);
        buzzerPlay(BuzzerPattern::Start, currentMillis);
        currentState = MENU_STATE_TARGET_MENU;
        displayNeedsUpdate = true;
      }
      break;


    case MENU_STATE_SCREENSAVER_EDIT:
    {
      constexpr int options[5] = {0, 2, 5, 15, 30};
      int curIdx = 2;
      for (int i = 0; i < 5; i++)
      {
        if (options[i] == configuredScreensaverTimeout)
        {
          curIdx = i;
          break;
        }
      }
      if (startPressed)
      {
        curIdx = (curIdx + 1) % 5;
        configuredScreensaverTimeout = options[curIdx];
        buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
        displayNeedsUpdate = true;
      }
      if (stopPressed)
      {
        curIdx = (curIdx - 1 + 5) % 5;
        configuredScreensaverTimeout = options[curIdx];
        buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
        displayNeedsUpdate = true;
      }
      if (resetShortPress)
      {
        storageSaveScreensaverTimeout(configuredScreensaverTimeout);
        roundControllerSetScreensaverTimeout(configuredScreensaverTimeout);
        buzzerPlay(BuzzerPattern::Start, currentMillis);
        currentState = MENU_STATE_TARGET_MENU;
        displayNeedsUpdate = true;
      }
      break;
    }

    case MENU_STATE_LANGUAGE_EDIT:
      if (startPressed || stopPressed)
      {
        configuredLanguage = (configuredLanguage == Language::DE) ? Language::EN : Language::DE;
        i18nSetLanguage(configuredLanguage);
        buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
        displayNeedsUpdate = true;
      }
      if (resetShortPress)
      {
        storageSaveLanguage((int)configuredLanguage);
        i18nSetLanguage(configuredLanguage);
        buzzerPlay(BuzzerPattern::Start, currentMillis);
        currentState = MENU_STATE_TARGET_MENU;
        displayNeedsUpdate = true;
      }
      break;


    case MENU_STATE_NET_MENU:
      if (startPressed)
      {
        netItemIndex = (netItemIndex - 1 + NET_ITEM_COUNT) % NET_ITEM_COUNT;
        buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
        displayNeedsUpdate = true;
      }
      if (stopPressed)
      {
        netItemIndex = (netItemIndex + 1) % NET_ITEM_COUNT;
        buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
        displayNeedsUpdate = true;
      }
      if (resetShortPress)
      {
        if (netItemIndex == 7) // WLAN verbinden
        {
          buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
          displayUpdate(STR_CONNECTING, STR_CANCEL_HINT);
          bool ok = networkReconnectWiFi(btnStop);
          if (ok)
          {
            buzzerPlay(BuzzerPattern::Start, currentMillis);
            displayUpdate(STR_WLAN_OK, STR_STATUS_CONNECTED);
            delay(1000);
          }
          else
          {
            buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
            displayUpdate(STR_WLAN_ERROR, STR_CONNECT_FAILED);
            delay(1000);
          }
          displayNeedsUpdate = true;
        }
        else if (netItemIndex == 8) // WLAN Setup (AP)
        {
          buzzerPlay(BuzzerPattern::Start, currentMillis);
          networkStartAPMode();
          menuActive = false;
          return false;
        }
        else if (netItemIndex == 9) // WPS Starten
        {
          buzzerPlay(BuzzerPattern::Start, currentMillis);
          networkStartWPS();
        }
        else if (netItemIndex == 10) // WLAN loeschen
        {
          buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
          currentState = MENU_STATE_WIFI_RESET_CONFIRM;
          displayNeedsUpdate = true;
        }
        else if (netItemIndex == 11) // Zurueck
        {
          buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
          currentState = MENU_STATE_MAIN_CAT;
          displayNeedsUpdate = true;
        }
        else
        {
          buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
        }
      }
      break;

    case MENU_STATE_REMOTE_PAIRING:
      if (resetShortPress)
      {
        remoteControlDisable();
        buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
        currentState = MENU_STATE_MAIN_CAT;
        displayNeedsUpdate = true;
      }
      break;

    case MENU_STATE_WIFI_RESET_CONFIRM:
      if (startPressed) // JA
      {
        buzzerPlay(BuzzerPattern::Start, currentMillis);
        displayUpdate(STR_RESET, STR_WIFI_CLEARED);
        networkResetCredentials();
        delay(1000);
        ESP.restart();
      }
      if (stopPressed || resetShortPress) // NEIN
      {
        buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
        currentState = MENU_STATE_NET_MENU;
        displayNeedsUpdate = true;
      }
      break;

    case MENU_STATE_TEST_MENU:
      if (startPressed)
      {
        testSelectIndex = (testSelectIndex - 1 + TEST_SELECT_COUNT) % TEST_SELECT_COUNT;
        buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
        displayNeedsUpdate = true;
      }
      if (stopPressed)
      {
        testSelectIndex = (testSelectIndex + 1) % TEST_SELECT_COUNT;
        buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
        displayNeedsUpdate = true;
      }
      if (resetShortPress)
      {
        buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
        switch (testSelectIndex)
        {
        case 0: // Sensor Live Matrix
          currentState = MENU_STATE_TEST_SENSORS;
          for (int i = 0; i < MAX_TARGETS; i++)
          {
            sensorMatrixHits[i] = false;
          }
          lastHitTargetIndex = -1;
          sensorIsrReset();
          sensorIsrSetTestMode(true);
          sensorIsrEnable(true);
          sensorIsrSetRunning(true);
          ledsReset();
          break;

        case 1: // LED & Buzzer Test
          currentState = MENU_STATE_TEST_LEDS_BUZZER;
          testTimer = currentMillis;
          testStep = 0;
          ledsReset();
          break;

        case 2: // Zurueck
          currentState = MENU_STATE_MAIN_CAT;
          break;
        }
        displayNeedsUpdate = true;
      }
      break;

    case MENU_STATE_TEST_SENSORS:
      if (resetShortPress)
      {
        // Short press on reset in sensor matrix clears the matrix hits
        for (int i = 0; i < MAX_TARGETS; i++)
        {
          sensorMatrixHits[i] = false;
        }
        lastHitTargetIndex = -1;
        ledsReset();
        buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
        displayNeedsUpdate = true;
      }
      else
      {
        int targetIndex = -1;
        unsigned long hitTime = 0;
        while (sensorIsrPopNextHit(targetIndex, hitTime))
        {
          lastHitTargetIndex = targetIndex;
          sensorMatrixHits[targetIndex] = true;
          ledTargetSet(targetIndex, true);
          buzzerPlay(BuzzerPattern::SingleShort, currentMillis);
          displayNeedsUpdate = true;
        }
      }
      break;

    case MENU_STATE_TEST_LEDS_BUZZER:
      if (resetShortPress)
      {
        stopRunningTest();
        buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
        currentState = MENU_STATE_TEST_MENU;
        displayNeedsUpdate = true;
      }
      else
      {
        if (startPressed || stopPressed)
        {
          buzzerPlay(BuzzerPattern::Start, currentMillis);
        }

        if (currentMillis - testTimer >= 300)
        {
          testTimer = currentMillis;
          testStep = (testStep + 1) % 6;
          for (int t = 0; t < MAX_TARGETS; t++)
          {
            ledTargetSet(t, t == testStep);
          }
          ledRedSet(testStep == 5);
        }
      }
      break;

    case MENU_STATE_INFO:
      if (resetShortPress)
      {
        buzzerPlay(BuzzerPattern::MenuClick, currentMillis);
        currentState = MENU_STATE_MAIN_CAT;
        displayNeedsUpdate = true;
      }
      break;
    }
  }

  static MenuState lastState = (MenuState)-1;
  static int lastMainCat = -1;
  static int lastTargetSub = -1;
  static int lastNetItem = -1;
  static int lastTestSelect = -1;
  static int lastTargetCount = -1;
  static bool lastRequireAll = false;
  static int lastRoundMode = -1;
  static int lastScreensaverTimeout = -1;

  if (displayNeedsUpdate || currentState != lastState ||
      (currentState == MENU_STATE_MAIN_CAT && mainCatIndex != lastMainCat) ||
      (currentState == MENU_STATE_TARGET_MENU && targetSubIndex != lastTargetSub) ||
      (currentState == MENU_STATE_TARGET_COUNT_EDIT && configuredTargetCount != lastTargetCount) ||
      (currentState == MENU_STATE_STOP_MODE_EDIT && (configuredRequireAllTargets != lastRequireAll || configuredRoundMode != lastRoundMode)) ||
      (currentState == MENU_STATE_SCREENSAVER_EDIT && configuredScreensaverTimeout != lastScreensaverTimeout) ||
      (currentState == MENU_STATE_NET_MENU && netItemIndex != lastNetItem) ||
      (currentState == MENU_STATE_TEST_MENU && testSelectIndex != lastTestSelect))
  {
    lastState = currentState;
    lastMainCat = mainCatIndex;
    lastTargetSub = targetSubIndex;
    lastTargetCount = configuredTargetCount;
    lastRequireAll = configuredRequireAllTargets;
    lastRoundMode = configuredRoundMode;
    lastScreensaverTimeout = configuredScreensaverTimeout;
    lastNetItem = netItemIndex;
    lastTestSelect = testSelectIndex;
    menuUpdateDisplay();
  }



  return true;
}
