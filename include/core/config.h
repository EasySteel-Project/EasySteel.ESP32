#pragma once

#include <Arduino.h>

// --- Feature Flags ---
#define USE_DISPLAY 1

// --- Built-in LED ---
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

// --- Network Settings ---
constexpr const char *AP_SSID_PREFIX = "EasySteel_"; // + last 4 MAC chars
constexpr uint16_t DEFAULT_TCP_PORT = 53421;                  // Default timing-event TCP server port
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 4000;

// --- Hardware Pins ---
constexpr int SENSOR_PINS[5] = {32, 33, 25, 26, 27}; // Index 0 (GPIO 32) = Stop Plate / Port 1
constexpr int LED_RED_PIN = 4;
constexpr int LED_GRN_PINS[5] = {19, 18, 5, 17, 16};
constexpr int BTN_START_PIN = 14;
constexpr int BTN_STOP_PIN = 12;
constexpr int BTN_RESET_PIN = 13;
constexpr int BUZZER_PIN = 23;

// --- Round Constants ---
constexpr int MAX_TARGETS = 5;
constexpr int MAX_RECORDED_HITS = 16;
constexpr unsigned long ROUND_TIMEOUT_MS = 30000;
constexpr unsigned long MENU_HOLD_MS = 1500;
constexpr unsigned long START_DELAY_MIN_MS = 1000;
constexpr unsigned long START_DELAY_MAX_MS = 4001; // exclusive upper bound for random()

// --- Timing ---
constexpr unsigned long SENSOR_DEBOUNCE_DELAY_MS = 50;
constexpr unsigned long GLOBAL_CROSSTALK_LIMIT_MS = 25;
constexpr unsigned long LED_BLINK_INTERVAL_MS = 1000;
constexpr unsigned long RED_BLINK_INTERVAL_MS = 100;
