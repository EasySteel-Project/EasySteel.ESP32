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

bool storageLoadRequireAllTargets()
{
  prefs.begin("easysteel", true);
  int mode = prefs.getInt("game_mode", -1);
  if (mode >= 0)
  {
    prefs.end();
    return mode == 1;
  }
  bool requireAll = prefs.getBool("require_all", false);
  prefs.end();
  return requireAll;
}

void storageSaveRequireAllTargets(bool requireAll)
{
  prefs.begin("easysteel", false);
  prefs.putBool("require_all", requireAll);
  prefs.putInt("game_mode", requireAll ? 1 : 0);
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
