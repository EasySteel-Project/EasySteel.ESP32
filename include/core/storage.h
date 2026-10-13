#pragma once

#include <Arduino.h>

// --- WiFi Credentials ---
bool storageLoadWiFi(String &ssid, String &pass);
void storageSaveWiFi(const String &ssid, const String &pass);
void storageClearWiFi();

// --- Device Settings ---
void storageLoadDevice(String &name, String &serverIp, uint16_t &port, const String &fallbackName);
void storageSaveDevice(const String &name, const String &serverIp, uint16_t port);
void storageClearDevice();

// --- Round Settings ---
int storageLoadTargetCount();
void storageSaveTargetCount(int count);
int storageLoadRoundMode();
void storageSaveRoundMode(int mode);
int storageLoadScreensaverTimeout();
void storageSaveScreensaverTimeout(int minutes);
int storageLoadPreparationDelay();
void storageSavePreparationDelay(int seconds);
int storageLoadLanguage();
void storageSaveLanguage(int language);

/// Load or create the stable device secret. Failure must disable network administration.
bool storageLoadDevicePassword(String& password, uint32_t (*randomWord)());
