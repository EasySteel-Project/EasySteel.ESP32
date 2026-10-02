#include "config.h"
#include "display.h"
#include "network.h"
#include "round_controller.h"
#include "ui_strings.h"
#include "button.h"
#include "buzzer.h"
#include "hw_context.h"
#include "watchdog.h"
#include <esp_sleep.h>

Button btnStart(BTN_START_PIN);
Button btnStop(BTN_STOP_PIN);
Button btnReset(BTN_RESET_PIN);

/// Grace period after boot before provisioning-mode button presses are accepted.
/// Prevents accidental exit when Reset is held during boot for WiFi clear.
static constexpr unsigned long BOOT_GRACE_PERIOD_MS = 1000;

/// Set once in setup(). Only transitions true→false (never back without reboot).
static bool provisioning = false;

/// Non-blocking input cooldown (replaces blocking delay(300)).
static unsigned long mainCooldownUntil = 0;
static constexpr unsigned long MAIN_COOLDOWN_MS = 300;

static void heartbeatTick(unsigned long currentMillis)
{
  static unsigned long lastToggle = 0;
  static bool state = false;
  unsigned long interval = provisioning ? LED_BLINK_INTERVAL_MS / 2 : LED_BLINK_INTERVAL_MS;

  if (currentMillis - lastToggle >= interval)
  {
    lastToggle = currentMillis;
    state = !state;
    digitalWrite(LED_BUILTIN, state ? HIGH : LOW);
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  // Initialize Watchdog Timer (4 seconds, panic reset on timeout)
  watchdogInit(4);

  btnStart.init();
  btnStop.init();
  btnReset.init();

  displayInit();

  HwButtons buttons{btnStart, btnStop, btnReset};
  roundControllerInit(buttons);

  // Hold Reset button continuously for 2s on boot → reset WiFi credentials (ignore if waking up from deep sleep!)
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT0)
  {
    constexpr unsigned long BOOT_RESET_HOLD_MS = 2000;
    unsigned long bootCheckStart = millis();
    bool clearWifi = false;

    while (btnReset.isDown())
    {
      watchdogFeed();
      unsigned long currentMillis = millis();
      btnReset.update(currentMillis);

      if (currentMillis - bootCheckStart >= BOOT_RESET_HOLD_MS)
      {
        clearWifi = true;
        break;
      }
      delay(10);
    }

    if (clearWifi)
    {
      displayUpdate(STR_RESET, STR_WIFI_CLEARED);
      Serial.println("Reset button held on boot for 2s - clearing WiFi credentials");
      networkResetCredentials();
      delay(2000);
    }
  }

  // Feed WDT before animation block
  watchdogFeed();

  // Play boot display animation (which triggers fanfare when fully assembled)
  displayShowStartupAnimation();

  // Feed WDT after animation block
  watchdogFeed();

  networkInit(&btnStop);
  provisioning = networkIsProvisioning();

  if (!provisioning)
  {
    displayUpdate(STR_READY, STR_PRESS_START);
  }
}

void loop()
{
  unsigned long currentMillis = millis();
  watchdogFeed();

  if (btnStart.update(currentMillis)) {
    Serial.printf("[BUTTON] Start state changed. IsDown: %d (GPIO: %d, PinVal: %d)\n", btnStart.isDown(), BTN_START_PIN, digitalRead(BTN_START_PIN));
  }
  if (btnStop.update(currentMillis)) {
    Serial.printf("[BUTTON] Stop state changed. IsDown: %d (GPIO: %d, PinVal: %d)\n", btnStop.isDown(), BTN_STOP_PIN, digitalRead(BTN_STOP_PIN));
  }
  if (btnReset.update(currentMillis)) {
    Serial.printf("[BUTTON] Reset state changed. IsDown: %d (GPIO: %d, PinVal: %d)\n", btnReset.isDown(), BTN_RESET_PIN, digitalRead(BTN_RESET_PIN));
  }

  heartbeatTick(currentMillis);
  networkTick(currentMillis);

  // Non-blocking input cooldown
  if (currentMillis < mainCooldownUntil)
  {
    return;
  }

  if (!provisioning)
  {
    roundControllerTick(currentMillis);
  }
  else
  {
    // Consume presses every tick to avoid stale edges from boot-held buttons
    bool anyPressed = btnStart.wasPressed() || btnStop.wasPressed() || btnReset.wasPressed();

    if (anyPressed && currentMillis > BOOT_GRACE_PERIOD_MS)
    {
      networkStopProvisioningAndSetManual();
      provisioning = false;
      mainCooldownUntil = currentMillis + MAIN_COOLDOWN_MS;
    }
  }
}
