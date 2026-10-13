#include "storage.h"
#include "config.h"

#include <Preferences.h>

static Preferences prefs;

// ============================================================
// WiFi Credentials (namespace: "wifi")
// ============================================================

bool storageLoadWiFi(String &ssid, String &pass)
{
  prefs.begin("wifi", true);
  ssid = prefs.getString("ssid", "");
  pass = prefs.getString("pass", "");
  prefs.end();
  return ssid.length() > 0;
}

void storageSaveWiFi(const String &ssid, const String &pass)
{
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
}

void storageClearWiFi()
{
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();
  Serial.println("WiFi credentials cleared");
}

// ============================================================
// Device Settings (namespace: "device")
// ============================================================

void storageLoadDevice(String &name, String &serverIp, uint16_t &port, const String &fallbackName)
{
  prefs.begin("device", true);
  name = prefs.getString("name", "");
  serverIp = prefs.getString("serverIp", "");
  port = prefs.getUShort("port", DEFAULT_TCP_PORT);
  prefs.end();

  if (name.length() == 0)
  {
    name = fallbackName;
  }
}

void storageSaveDevice(const String &name, const String &serverIp, uint16_t port)
{
  prefs.begin("device", false);
  prefs.putString("name", name);
  prefs.putString("serverIp", serverIp);
  prefs.putUShort("port", port);
  prefs.end();
}

void storageClearDevice()
{
  prefs.begin("device", false);
  prefs.clear();
  prefs.end();
  Serial.println("Device settings cleared");
}

// ============================================================
// Round settings (namespace: "easysteel")
// ============================================================

int storageLoadTargetCount()
{
  prefs.begin("easysteel", true);
  int count = prefs.getInt("targets", MAX_TARGETS);
  prefs.end();
  return count;
}

void storageSaveTargetCount(int count)
{
  prefs.begin("easysteel", false);
  prefs.putInt("targets", count);
  prefs.end();
}

int storageLoadRoundMode()
{
  prefs.begin("easysteel", true);
  int mode = prefs.getInt("game_mode", -1);
  if (mode < 0)
  {
    bool requireAll = prefs.getBool("require_all", false);
    mode = requireAll ? 1 : 0;
  }
  prefs.end();
  return mode;
}

void storageSaveRoundMode(int mode)
{
  prefs.begin("easysteel", false);
  prefs.putInt("game_mode", mode);
  prefs.putBool("require_all", mode == 1);
  prefs.end();
}


int storageLoadScreensaverTimeout()
{
  prefs.begin("easysteel", true);
  int timeout = prefs.getInt("screensaver", 5); // Default 5 minutes
  prefs.end();
  return timeout;
}

void storageSaveScreensaverTimeout(int minutes)
{
  prefs.begin("easysteel", false);
  prefs.putInt("screensaver", minutes);
  prefs.end();
}

int storageLoadPreparationDelay()
{
  prefs.begin("easysteel", true);
  int delay = prefs.getInt("prep_delay", 0);
  prefs.end();
  if (delay < 0 || delay > 30)
  {
    delay = 0;
  }
  return delay;
}

void storageSavePreparationDelay(int seconds)
{
  if (seconds < 0)
  {
    seconds = 0;
  }
  else if (seconds > 30)
  {
    seconds = 30;
  }
  prefs.begin("easysteel", false);
  prefs.putInt("prep_delay", seconds);
  prefs.end();
}

int storageLoadLanguage()
{
  prefs.begin("easysteel", true);
  int lang = prefs.getInt("lang", 0); // Default 0 (DE)
  prefs.end();
  return lang;
}

void storageSaveLanguage(int language)
{
  prefs.begin("easysteel", false);
  prefs.putInt("lang", language);
  prefs.end();
}

bool storageLoadDevicePassword(String& password, uint32_t (*randomWord)())
{
  password = "";
  if (!prefs.begin("security", false)) { return false; }
  String stored = prefs.getString("password", "");
  if (stored.length() == 0 && randomWord)
  {
    char generated[17];
    snprintf(generated, sizeof(generated), "%08lx%08lx",
      static_cast<unsigned long>(randomWord()), static_cast<unsigned long>(randomWord()));
    if (prefs.putString("password", generated) != 16)
    {
      prefs.end();
      return false;
    }
    stored = prefs.getString("password", "");
  }
  prefs.end();
  if (stored.length() != 16) { return false; }
  for (size_t index = 0; index < stored.length(); index++)
  {
    const char value = stored[index];
    if (!((value >= '0' && value <= '9') || (value >= 'a' && value <= 'f'))) { return false; }
  }
  password = stored;
  return true;
}
