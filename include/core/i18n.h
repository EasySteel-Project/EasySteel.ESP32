#pragma once

#include <stdint.h>
#include <stddef.h>

enum class Language : uint8_t
{
  DE = 0,
  EN = 1,
  COUNT
};

struct DisplayStrings
{
  // --- General & System ---
  const char *appName;
  const char *booting;
  const char *reset;
  const char *wifiCleared;
  const char *firmwareVer;
  const char *subtitleTimingSystem;

  // --- Round States & Alerts ---
  const char *ready;
  const char *readyForStart;
  const char *pressStart;
  const char *go;
  const char *finish;
  const char *prepare;
  const char *waitForBeep;
  const char *timeout;
  const char *falseStart;
  const char *falseStartHint;
  const char *reaction;
  const char *reactionTimeSubtitle;
  const char *drillHint;
  const char *outboxPendingFormat;
  const char *outboxFull;
  const char *outboxStorageError;
  const char *outboxBlocked;
  const char *outboxKeepResult;
  const char *outboxKeepPower;
  const char *outboxConnect;
  const char *outboxService;

  // --- Target & Split Formatting ---
  const char *stopPlate;
  const char *targetN;
  const char *hitStop;
  const char *hitTarget;
  const char *hitStopFormat;
  const char *hitTargetFormat;
  const char *splitPrefix;
  const char *penaltyMissFormat;
  const char *lastTimeFormat;
  const char *lastTimePenaltyFormat;
  const char *timeFormat;
  const char *runningTimeFormat;
  const char *unitSeconds;
  const char *timeValueFormat;
  const char *runningTimeValueFormat;
  const char *initialTimeValue;

  // --- Mode Tags ---
  const char *modeTagDrill;
  const char *modeTagAll;
  const char *modeTagWk;
  const char *modeTagTargetCountFormat;

  // --- Footers ---
  const char *footerNav;
  const char *footerNavRun;
  const char *footerEdit;
  const char *footerConfirm;
  const char *footerBack;
  const char *footerMatrix;
  const char *footerFinished;
  const char *footerSplit;
  const char *footerStandby;
  const char *footerStandbyDrill;
  const char *footerReaction;
  const char *back;
  const char *menuPreviousGlyph;
  const char *menuSelectedGlyph;
  const char *menuNextGlyph;

  // --- Menu Categories & Titles ---
  const char *titleMenu;
  const char *titleSetting;
  const char *titleConfirm;
  const char *titleSysinfo;
  const char *titleSensorMatrix;

  const char *catTargets;
  const char *catPhoneTraining;
  const char *catNetwork;
  const char *catTest;
  const char *catInfo;
  const char *catExit;

  const char *menuTitleMain;
  const char *menuTitleTargets;
  const char *menuTitleNetwork;
  const char *menuTitleTests;

  const char *targetCount;
  const char *stopMode;
  const char *modeCompetition;
  const char *modeAllTargets;
  const char *modeReaction;
  const char *prepDelay;
  const char *screensaver;
  const char *timeoutOff;
  const char *language;

  const char *titleTargetCount;
  const char *paramTargetCount;
  const char *titleGameMode;
  const char *paramMode;
  const char *hintGameMode;
  const char *titlePrepDelay;
  const char *paramPrepDelay;
  const char *hintPrepDelay;
  const char *titleScreensaver;
  const char *paramScreensaver;
  const char *hintScreensaver;
  const char *titleLanguage;
  const char *paramLanguage;
  const char *langDe;
  const char *langEn;

  const char *valueIntFormat;
  const char *valueTextFormat;
  const char *valueMinutesFormat;
  const char *valueSecondsFormat;
  const char *prepCountdownFormat;

  // --- Hardware Test ---
  const char *testSensors;
  const char *testLedsBuzzer;
  const char *titleLedBuzzer;
  const char *paramLedBuzzer;
  const char *hintLedBuzzer;
  const char *sensorMatrixHint;
  const char *sensorLastStop;
  const char *sensorLastTarget;
  const char *stopPlateShort;
  const char *testClearOutbox;
  const char *confirmClearOutboxTitle;
  const char *confirmClearOutboxLine1;
  const char *confirmClearOutboxLine2;

  // --- Network & WiFi ---
  const char *wifiStatus;
  const char *statusConnected;
  const char *statusDisconnected;
  const char *statusAp;
  const char *ssid;
  const char *localIp;
  const char *serverIp;
  const char *port;
  const char *deviceName;
  const char *actionWifiConnect;
  const char *actionApMode;
  const char *actionWps;
  const char *actionWifiReset;
  const char *connectOk;
  const char *connectFailed;
  const char *cancelHint;
  const char *wlanOk;
  const char *wlanError;

  const char *confirmWifiTitle;
  const char *confirmWifiLine1;
  const char *confirmWifiLine2;

  const char *wifiSetup;
  const char *connecting;
  const char *wpsOk;
  const char *saved;
  const char *restarting;
  const char *wpsFailed;
  const char *wpsTimeout;
  const char *wpsActive;
  const char *pressRouterWps;
  const char *manualMode;
  const char *wifiStopOffline;
  const char *wifiApNameFormat;
  const char *wifiIpFormat;
  const char *wifiPasswordFormat;
  const char *networkStatusFormat;
  const char *networkSsidFormat;
  const char *networkIpFormat;
  const char *networkServerFormat;
  const char *networkPortFormat;
  const char *networkNameFormat;
  const char *networkIdFormat;
  const char *menuActionFormat;
  const char *networkApGlyph;
  const char *networkOfflineGlyph;
  const char *deviceInfoFwFormat;
  const char *deviceInfoIpFormat;
  const char *deviceInfoUptimeFormat;
  const char *firmwareFallback;
  const char *valueUnavailable;
  const char *remotePairingTitle;
  const char *remotePairingPin;
  const char *remotePairingHint;
};

struct WebStrings
{
  // --- WiFi Setup & Settings Web UI ---
  const char *webWifiSetupTitle;
  const char *webWifiSetupHeading;
  const char *webWifiSetupSubtitle;
  const char *webAvailableNetworks;
  const char *webRescan;
  const char *webScanningNetworks;
  const char *webWifiName;
  const char *webNetworkName;
  const char *webPassword;
  const char *webWifiPassword;
  const char *webSaveAndConnect;
  const char *webOr;
  const char *webWpsHint;
  const char *webWpsStart;
  const char *webNoNetworks;
  const char *webSelectNetworkPrefix;
  const char *webSelectNetworkSuffix;
  const char *webManualEntry;
  const char *webSignalUnit;
  const char *webWpsPageTitle;
  const char *webWpsActive;
  const char *webWpsWaiting;
  const char *webSuccessPageTitle;
  const char *webSuccessHeading;
  const char *webSettingsTitle;
  const char *webSettingsHeading;
  const char *webSettingsSubtitle;
  const char *webDeviceName;
  const char *webIp;
  const char *webPort;
  const char *webPrepDelay;
  const char *webServerIpPlaceholder;
  const char *webSaveSettings;
  const char *webFirmwareUpdate;
  const char *webFlashFirmware;
  const char *webResetWifiConfirm;
  const char *webResetWifi;
  const char *webInstallFirmwarePrefix;
  const char *webInstallFirmwareSuffix;
  const char *webUploadRunning;
  const char *webFlashingPrefix;
  const char *webUpdateSuccess;
  const char *webErrorPrefix;
  const char *webConnectionInterrupted;
  const char *webPercentUnit;
  const char *webWifiResetMessage;
  const char *webWifiRestartMessage;
  const char *webSettingsSavedMessage;
  const char *webAuthenticationRequired;
  const char *webMethodNotAllowed;
  const char *webSsidRequired;
  const char *webOtaIdleRequired;
  const char *webOtaDisplayTitle;
  const char *webOtaErrorTitle;
  const char *webOtaSuccess;
  const char *webOtaAbort;
  const char *webOtaBrowserTitle;
  const char *webOtaFlashing;
  const char *webApiStateOverflow;
  const char *webApiInvalidJson;
  const char *webApiInvalidServerIp;
  const char *webApiPairingAttempts;
  const char *webApiInvalidPin;
  const char *webApiPairingInactive;
  const char *webApiPhoneNotPaired;
  const char *webApiActionNotAllowed;
  const char *webApiCommandBusy;

  // --- Mobile Training Web UI ---
  const char *trainingPageTitle;
  const char *trainingWakeOff;
  const char *trainingWakeOn;
  const char *trainingBeepOff;
  const char *trainingBeepOn;
  const char *trainingPcLocked;
  const char *trainingReleaseRequired;
  const char *trainingPinPlaceholder;
  const char *trainingPair;
  const char *trainingTargetPrefix;
  const char *trainingStart;
  const char *trainingStop;
  const char *trainingCountdownStart;
  const char *trainingRoundStop;
  const char *trainingReset;
  const char *trainingCurrentSplits;
  const char *trainingNoHits;
  const char *trainingStage;
  const char *trainingLocalOnly;
  const char *trainingStage5ToGo;
  const char *trainingStageShowdown;
  const char *trainingStageSmokeAndHope;
  const char *trainingStageOuterLimits;
  const char *trainingStageAccelerator;
  const char *trainingStageThePendulum;
  const char *trainingStageSpeedOption;
  const char *trainingStageRoundabout;
  const char *trainingPrepDelay;
  const char *trainingPrepDelayHint;
  const char *trainingPrepDelayOff;
  const char *trainingPrepare;
  const char *trainingHistoryLocal;
  const char *trainingRuns;
  const char *trainingAll;
  const char *trainingBestTime;
  const char *trainingAverage;
  const char *trainingNoRunsSaved;
  const char *trainingCopyCsv;
  const char *trainingDeleteHistory;
  const char *trainingSettingsLink;
  const char *trainingUnknown;
  const char *trainingNoRunsCategory;
  const char *trainingNoEntries;
  const char *trainingCsvHeader;
  const char *trainingCsvCopied;
  const char *trainingCsvPrompt;
  const char *trainingClearConfirm;
  const char *trainingPinInvalid;
  const char *trainingPairFailed;
  const char *trainingController;
  const char *trainingPairPrompt;
  const char *trainingOtherPaired;
  const char *trainingPcPriority;
  const char *trainingReleaseMainMenu;
  const char *trainingNextRun;
  const char *trainingAbortCountdown;
  const char *trainingBackToStandby;
  const char *trainingDiscardRun;
  const char *trainingPenaltyLabel;
  const char *trainingTargetLabel;
  const char *trainingMissAbbreviation;
  const char *trainingDisconnect;
};

const DisplayStrings &i18n();
const WebStrings &i18nWeb();
Language i18nGetLanguage();
void i18nSetLanguage(Language lang);
const char *i18nGetLanguageCode();
const char *i18nGetLanguageName();
