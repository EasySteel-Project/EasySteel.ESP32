#include "display.h"
#include "strings.h"
#include "buzzer.h"
#include "leds.h"
#include "watchdog.h"

#if USE_DISPLAY
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

static Adafruit_SH1106G oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
static bool connected = false;
#endif

bool displayInit()
{
#if USE_DISPLAY
  if (oled.begin(0x3C, true))
  {
    connected = true;
    displayUpdate(STR_BOOTING, "");
    return true;
  }
  Serial.println(F("SH1106 display not found - running displayless"));
#endif
  return false;
}

#if USE_DISPLAY
#include "network.h"
#include <WiFi.h>

/// Draw the WiFi RSSI signal strength bars and TCP status dot.
static void drawNetworkStatus()
{
  if (networkIsProvisioning())
  {
    oled.setCursor(95, 0);
    oled.print("AP");
    return;
  }

  if (!networkIsWiFiConnected())
  {
    // Draw an 'x' representing offline state
    oled.setCursor(95, 0);
    oled.print("x");
    return;
  }

  // Draw WiFi RSSI bars
  int32_t rssi = WiFi.RSSI();
  int bars = 0;
  if (rssi >= -55) bars = 4;
  else if (rssi >= -70) bars = 3;
  else if (rssi >= -85) bars = 2;
  else if (rssi >= -100) bars = 1;

  int startX = 105;
  for (int i = 0; i < 4; i++)
  {
    int barHeight = (i + 1) * 2;
    int x = startX + i * 3;
    int y = 8 - barHeight;
    if (i < bars)
    {
      oled.fillRect(x, y, 2, barHeight, SH110X_WHITE);
    }
    else
    {
      oled.drawRect(x, y, 2, barHeight, SH110X_WHITE);
    }
  }

  // Draw TCP connection status dot
  if (networkIsServerConnected())
  {
    oled.fillCircle(123, 4, 3, SH110X_WHITE);
  }
  else
  {
    oled.drawCircle(123, 4, 3, SH110X_WHITE);
  }
}

/// Draw the shared header (clear + "EasySteel" title + divider line).
static void drawHeader()
{
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SH110X_WHITE);
  oled.setCursor(0, 0);
  oled.print("EasySteel");
  drawNetworkStatus();
  oled.drawLine(0, 10, 127, 10, SH110X_WHITE);
}
#endif

void displayUpdate(const char *line1, const char *line2)
{
#if USE_DISPLAY
  if (!connected)
    return;
  drawHeader();

  // Primary status
  if (strlen(line1) <= 10)
  {
    oled.setTextSize(2);
    oled.setCursor(0, 16);
  }
  else
  {
    oled.setTextSize(1);
    oled.setCursor(0, 20);
  }
  oled.println(line1);

  // Secondary details
  oled.setTextSize(1);
  oled.setCursor(0, 44);
  oled.println(line2);

  oled.display();
#endif
}

static void drawTargetMiniBoxes(int y, const bool hitMask[5], int targetCount)
{
  if (targetCount < 1) targetCount = 1;
  if (targetCount > 5) targetCount = 5;

  const int boxW = 18;
  const int boxH = 9;
  const int gap = 4;
  int totalW = targetCount * boxW + (targetCount - 1) * gap;
  int startX = (128 - totalW) / 2;

  for (int i = 0; i < targetCount; i++)
  {
    int targetId = (i < targetCount - 1) ? (i + 1) : 0;
    bool isHit = hitMask ? hitMask[targetId] : false;
    int bx = startX + i * (boxW + gap);

    if (isHit)
    {
      oled.fillRect(bx, y, boxW, boxH, SH110X_WHITE);
      oled.setTextColor(SH110X_BLACK, SH110X_WHITE);
    }
    else
    {
      oled.drawRect(bx, y, boxW, boxH, SH110X_WHITE);
      oled.setTextColor(SH110X_WHITE, SH110X_BLACK);
    }

    oled.setTextSize(1);
    oled.setCursor(bx + ((targetId == 0) ? 3 : 6), y + 1);
    if (targetId == 0)
    {
      oled.print("ST");
    }
    else
    {
      oled.print(targetId);
    }
  }
  oled.setTextColor(SH110X_WHITE, SH110X_BLACK);
}

void displayUpdateStandby(const char *deviceName, const char *ipAddress, bool serverConnected, int targetCount, int roundMode)
{
#if USE_DISPLAY
  if (!connected)
    return;
  drawHeader();

  // Lane / Device Name + Mode Tag
  oled.setTextSize(1);
  oled.setTextColor(SH110X_WHITE);
  oled.setCursor(0, 13);
  if (deviceName && strlen(deviceName) > 0)
  {
    oled.print(deviceName);
  }
  else
  {
    oled.print("EasySteel");
  }

  // Right-aligned Match config tag (e.g. "WK (5Z)" or "Alle (5Z)" or "Drill")
  char modeTag[16];
  if (roundMode == 2)
  {
    snprintf(modeTag, sizeof(modeTag), "Drill");
  }
  else
  {
    snprintf(modeTag, sizeof(modeTag), "%s(%dZ)", (roundMode == 1) ? "Alle" : "WK", targetCount);
  }
  int tagLen = strlen(modeTag);
  oled.setCursor(128 - tagLen * 6, 13);
  oled.print(modeTag);

  // Large "BEREIT" status (Size 2)
  oled.setTextSize(2);
  oled.setCursor(0, 24);
  oled.print((roundMode == 2) ? "DRILL" : STR_READY);

  // Bottom info: IP / Status
  oled.setTextSize(1);
  oled.setCursor(0, 44);
  if (roundMode == 2)
  {
    oled.print("Ziehen & Stopp treffen");
  }
  else if (ipAddress && strlen(ipAddress) > 0)
  {
    oled.print("IP: ");
    oled.print(ipAddress);
  }
  else
  {
    oled.print("Modus: Offline");
  }

  // Footer navigation hint
  oled.drawLine(0, 54, 127, 54, SH110X_WHITE);
  oled.setCursor(0, 56);
  if (roundMode == 2)
  {
    oled.print("Start: Drill starten");
  }
  else if (serverConnected)
  {
    oled.print("Start: Los  Menu:St+Rs");
  }
  else
  {
    oled.print("Start: Standalone");
  }

  oled.display();
#endif
}

void displayUpdateReaction(const char *timeStr, int hitTarget)
{
#if USE_DISPLAY
  if (!connected)
    return;
  drawHeader();

  // Top header: REAKTION / DRAW TIME
  oled.setTextSize(1);
  oled.setTextColor(SH110X_WHITE);
  oled.setCursor(0, 12);
  oled.print(STR_REACTION);

  if (hitTarget >= 0)
  {
    char tgtBuf[16];
    if (hitTarget == 0) snprintf(tgtBuf, sizeof(tgtBuf), "Stopp-Platte");
    else snprintf(tgtBuf, sizeof(tgtBuf), "Ziel %d", hitTarget);
    oled.setCursor(128 - (int)strlen(tgtBuf) * 6, 12);
    oled.print(tgtBuf);
  }

  // Big Time (Size 3)
  int len = strlen(timeStr);
  int size = (len > 6) ? 2 : 3;
  oled.setTextSize(size);
  int charWidth = size * 6;
  int totalWidth = len * charWidth;
  int x = (128 - totalWidth) / 2;
  if (x < 0) x = 0;
  oled.setCursor(x, 24);
  oled.print(timeStr);

  // Subtitle / Rating prompt
  oled.setTextSize(1);
  oled.setCursor(0, 46);
  oled.print("Reaktionszeit!");

  // Footer legend
  oled.drawLine(0, 55, 127, 55, SH110X_WHITE);
  oled.setCursor(0, 56);
  oled.print("Start: Nochmals  Rs:Ende");

  oled.display();
#endif
}


void displayUpdateRunning(const char *timeStr, float progress, const bool hitMask[5], int targetCount)
{
#if USE_DISPLAY
  if (!connected)
    return;
  drawHeader();

  // Top subheader: SHOOT! + Hit count
  oled.setTextSize(1);
  oled.setTextColor(SH110X_WHITE);
  oled.setCursor(0, 12);
  oled.print(STR_GO);

  int hitCount = 0;
  if (hitMask)
  {
    for (int i = 0; i < targetCount; i++)
    {
      int targetId = (i < targetCount - 1) ? (i + 1) : 0;
      if (hitMask[targetId]) hitCount++;
    }
  }
  char countBuf[16];
  snprintf(countBuf, sizeof(countBuf), "%d/%d", hitCount, targetCount);
  oled.setCursor(128 - (int)strlen(countBuf) * 6, 12);
  oled.print(countBuf);

  // Big Running Time (Size 3)
  int len = strlen(timeStr);
  int size = (len > 6) ? 2 : 3;
  oled.setTextSize(size);
  int charWidth = size * 6;
  int totalWidth = len * charWidth;
  int x = (128 - totalWidth) / 2;
  if (x < 0) x = 0;
  oled.setCursor(x, 22);
  oled.print(timeStr);

  // Mini target boxes at y=46
  drawTargetMiniBoxes(46, hitMask, targetCount);

  // 30s timeout progress bar at the very bottom (y=58..63)
  if (progress >= 0.0f)
  {
    if (progress > 1.0f) progress = 1.0f;
    oled.drawRect(0, 58, 128, 6, SH110X_WHITE);
    int barWidth = (int)(progress * 124.0f);
    if (barWidth > 0)
    {
      oled.fillRect(2, 60, barWidth, 2, SH110X_WHITE);
    }
  }

  oled.display();
#endif
}

void displayUpdateFinished(const char *timeStr, int penaltyCount, const bool hitMask[5], int targetCount, const char *footer)
{
#if USE_DISPLAY
  if (!connected)
    return;
  drawHeader();

  // Top header: FERTIG / TIMEOUT
  oled.setTextSize(1);
  oled.setTextColor(SH110X_WHITE);
  oled.setCursor(0, 12);
  oled.print(STR_FINISH);

  if (penaltyCount > 0)
  {
    char penBuf[16];
    snprintf(penBuf, sizeof(penBuf), "+%ds Miss", penaltyCount * 3);
    oled.setCursor(128 - (int)strlen(penBuf) * 6, 12);
    oled.print(penBuf);
  }

  // Big Result Time (Size 3)
  int len = strlen(timeStr);
  int size = (len > 6) ? 2 : 3;
  oled.setTextSize(size);
  int charWidth = size * 6;
  int totalWidth = len * charWidth;
  int x = (128 - totalWidth) / 2;
  if (x < 0) x = 0;
  oled.setCursor(x, 22);
  oled.print(timeStr);

  // Target mini boxes at y=45
  drawTargetMiniBoxes(45, hitMask, targetCount);

  // Footer legend at y=56
  oled.drawLine(0, 55, 127, 55, SH110X_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 56);
  oled.print(footer ? footer : STR_FOOTER_FINISHED);

  oled.display();
#endif
}

void displayUpdateSplitDetail(int hitIndex, int totalHits, int targetId, const char *splitTimeStr, const char *deltaStr, int penaltyCount)
{
#if USE_DISPLAY
  if (!connected)
    return;
  drawHeader();

  // Subheader: Hit 2/5 (Ziel 3) or (Stopp-Platte)
  oled.setTextSize(1);
  oled.setTextColor(SH110X_WHITE);
  oled.setCursor(0, 12);
  char headBuf[28];
  if (targetId == 0)
  {
    snprintf(headBuf, sizeof(headBuf), "Hit %d/%d (Stopp-Platte)", hitIndex, totalHits);
  }
  else
  {
    snprintf(headBuf, sizeof(headBuf), "Hit %d/%d (Ziel %d)", hitIndex, totalHits, targetId);
  }
  oled.print(headBuf);

  // Big Split Time (Size 3)
  int len = strlen(splitTimeStr);
  int size = (len > 6) ? 2 : 3;
  oled.setTextSize(size);
  int charWidth = size * 6;
  int totalWidth = len * charWidth;
  int x = (128 - totalWidth) / 2;
  if (x < 0) x = 0;
  oled.setCursor(x, 24);
  oled.print(splitTimeStr);

  // Split delta at y=46
  oled.setTextSize(1);
  oled.setCursor(0, 46);
  if (deltaStr && strlen(deltaStr) > 0)
  {
    oled.print("Split: +");
    oled.print(deltaStr);
    oled.print("s");
  }

  // Footer legend at y=56
  oled.drawLine(0, 55, 127, 55, SH110X_WHITE);
  oled.setCursor(0, 56);
  oled.print("Start:Weiter  Reset:Zurueck");

  oled.display();
#endif
}

void displayUpdateBigTime(const char *label, const char *numStr, const char *suffix, float progress, int penaltyCount)
{
#if USE_DISPLAY
  if (!connected)
    return;
  drawHeader();

  // Label (small, size 1) at y = 14
  oled.setTextSize(1);
  oled.setCursor(0, 14);
  oled.println(label);

  // Big time value
  int len = strlen(numStr);
  int size = 3;
  if (len > 6)
  {
    size = 2;
  }
  oled.setTextSize(size);

  // Center digits
  int charWidth = size * 6;
  int totalWidth = len * charWidth;
  int x = (128 - totalWidth) / 2;
  if (x < 0) x = 0;

  oled.setCursor(x, 28);
  oled.print(numStr);

  // Suffix (size 1) next to digits aligned to bottom
  if (suffix && strlen(suffix) > 0)
  {
    oled.setTextSize(1);
    oled.setCursor(x + totalWidth + 2, 28 + (size * 8) - 8);
    oled.print(suffix);
  }

  // Render 10 penalty dots at the bottom if penaltyCount >= 0
  if (penaltyCount >= 0)
  {
    if (penaltyCount > 10) penaltyCount = 10;
    for (int i = 0; i < 10; i++)
    {
      int dotX = 10 + i * 12;
      int dotY = 58;
      if (i < penaltyCount)
      {
        oled.fillCircle(dotX, dotY, 2, SH110X_WHITE);
      }
      else
      {
        oled.drawCircle(dotX, dotY, 2, SH110X_WHITE);
      }
    }
  }
  // Progress bar at the bottom if progress >= 0.0f
  else if (progress >= 0.0f)
  {
    if (progress > 1.0f) progress = 1.0f;
    oled.drawRect(0, 56, 128, 7, SH110X_WHITE);
    int barWidth = (int)(progress * 124.0f);
    if (barWidth > 0)
    {
      oled.fillRect(2, 58, barWidth, 3, SH110X_WHITE);
    }
  }

  oled.display();
#endif
}


void displayShowStartupAnimation()
{
#if USE_DISPLAY
  if (!connected)
    return;

  // Local helper for non-blocking buzzer updates during boot animation delay
  auto animationDelay = [](unsigned long ms) {
    unsigned long start = millis();
    while (millis() - start < ms)
    {
      watchdogFeed();
      buzzerTick(millis());
      delay(1);
    }
  };

  // Turn red LED on at the very start
  ledRedSet(true);

  // Step 1: Expanding target in center
  for (int r = 2; r <= 22; r += 3)
  {
    oled.clearDisplay();
    oled.drawCircle(64, 26, r, SH110X_WHITE);
    // Draw crosshair lines
    oled.drawLine(64 - r - 4, 26, 64 - r + 1, 26, SH110X_WHITE);
    oled.drawLine(64 + r - 1, 26, 64 + r + 4, 26, SH110X_WHITE);
    oled.drawLine(64, 26 - r - 4, 64, 26 - r + 1, SH110X_WHITE);
    oled.drawLine(64, 26 + r - 1, 64, 26 + r + 4, SH110X_WHITE);
    oled.display();
    animationDelay(40);
  }

  animationDelay(100);

  // Turn red LED off before starting the green LED sequence
  ledRedSet(false);

  // Step 2: Clear and reveal "EasySteel" text & underline growing
  for (int w = 0; w <= 54; w += 6)
  {
    oled.clearDisplay();
    oled.setTextSize(2);
    oled.setTextColor(SH110X_WHITE);
    oled.setCursor(10, 16);
    oled.print("EasySteel");

    // Draw growing underline
    oled.drawLine(64 - w, 36, 64 + w, 36, SH110X_WHITE);
    oled.display();
    animationDelay(30);
  }

  // Crisp clack sound when "EasySteel" locks into place, and light up target 1 green
  digitalWrite(BUZZER_PIN, HIGH);
  delay(15);
  digitalWrite(BUZZER_PIN, LOW);
  ledTargetSet(0, true);

  animationDelay(150);

  // Step 3: Slide up "TIMING SYSTEM" subtitle from bottom
  for (int yOffset = 12; yOffset >= 0; yOffset -= 3)
  {
    oled.clearDisplay();
    oled.setTextSize(2);
    oled.setTextColor(SH110X_WHITE);
    oled.setCursor(10, 16);
    oled.print("EasySteel");
    oled.drawLine(10, 36, 118, 36, SH110X_WHITE);

    oled.setTextSize(1);
    oled.setCursor(25, 46 + yOffset);
    oled.print("TIMING SYSTEM");
    oled.display();

    if (yOffset == 9)
    {
      ledTargetSet(1, true); // Light up target 2 green mid-slide
    }
    else if (yOffset == 3)
    {
      ledTargetSet(2, true); // Light up target 3 green mid-slide
    }

    animationDelay(40);
  }

  // Crisp clack sound when "TIMING SYSTEM" locks into place, and light up target 4 green
  digitalWrite(BUZZER_PIN, HIGH);
  delay(15);
  digitalWrite(BUZZER_PIN, LOW);
  ledTargetSet(3, true);

  animationDelay(300); // Brief pause before fanfare starts

  // Turn on all 5 green LEDs right before fanfare starts
  ledTargetSet(4, true);
  animationDelay(200);

  // Play the longer, punchy startup melody now that the logo is fully assembled
  buzzerPlay(BuzzerPattern::StartupMelody, millis());

  // Knight Rider / Cylon ping-pong running light pattern on target LEDs during fanfare melody
  int chasePos = 0;
  int direction = 1;
  for (int i = 0; i < 20; i++) // 20 steps * 100ms = 2000ms
  {
    // Clear all target LEDs and light up only the chase target
    for (int t = 0; t < 5; t++)
    {
      ledTargetSet(t, t == chasePos);
    }

    animationDelay(100);

    chasePos += direction;
    if (chasePos == 4 || chasePos == 0)
    {
      direction = -direction;
    }
  }

  // Turn all LEDs off at the end of the boot sequence
  ledsReset();
#endif
}

void displayUpdateMenuList(const char *headerTitle, const char *const *items, int itemCount, int selectedIndex, const char *footer)
{
#if USE_DISPLAY
  if (!connected)
    return;
  drawHeader();

  oled.setTextSize(1);
  oled.setTextColor(SH110X_WHITE);

  // Category Header / Breadcrumb at y = 13
  oled.setCursor(0, 13);
  oled.print(headerTitle ? headerTitle : "MENUE");

  // Previous item at y = 23
  if (selectedIndex > 0 && selectedIndex - 1 < itemCount)
  {
    oled.setCursor(8, 23);
    char buf[22];
    snprintf(buf, sizeof(buf), "%.20s", items[selectedIndex - 1]);
    oled.print(buf);
    oled.setCursor(120, 23);
    oled.print("^");
  }

  // Selected item (highlighted) at y = 33
  if (selectedIndex >= 0 && selectedIndex < itemCount)
  {
    oled.fillRect(0, 32, 128, 10, SH110X_WHITE);
    oled.setTextColor(SH110X_BLACK, SH110X_WHITE);
    oled.setCursor(2, 33);
    oled.print(">");
    oled.setCursor(8, 33);
    char buf[22];
    snprintf(buf, sizeof(buf), "%.20s", items[selectedIndex]);
    oled.print(buf);
    oled.setTextColor(SH110X_WHITE);
  }

  // Next item at y = 43
  if (selectedIndex + 1 < itemCount)
  {
    oled.setCursor(8, 43);
    char buf[22];
    snprintf(buf, sizeof(buf), "%.20s", items[selectedIndex + 1]);
    oled.print(buf);
    oled.setCursor(120, 43);
    oled.print("v");
  }

  // Divider and Footer
  oled.drawLine(0, 53, 127, 53, SH110X_WHITE);
  oled.setCursor(0, 55);
  oled.print(footer ? footer : STR_FOOTER_NAV);

  oled.display();
#endif
}

void displayUpdateValueEdit(const char *categoryTitle, const char *paramName, const char *valueStr, const char *hint, const char *footer)
{
#if USE_DISPLAY
  if (!connected)
    return;
  drawHeader();

  oled.setTextSize(1);
  oled.setTextColor(SH110X_WHITE);

  // Category Title
  oled.setCursor(0, 13);
  oled.print(categoryTitle ? categoryTitle : "EINSTELLUNG");

  // Parameter Name
  oled.setCursor(0, 23);
  oled.print(paramName ? paramName : "");

  // Value Display
  if (valueStr)
  {
    int len = strlen(valueStr);
    if (len <= 6)
    {
      oled.setTextSize(2);
      int x = (128 - (len * 12)) / 2;
      if (x < 0) x = 0;
      oled.setCursor(x, 34);
      oled.print(valueStr);
      oled.setTextSize(1);
    }
    else
    {
      oled.fillRect(8, 33, 112, 11, SH110X_WHITE);
      oled.setTextColor(SH110X_BLACK, SH110X_WHITE);
      int x = (128 - (len * 6)) / 2;
      if (x < 10) x = 10;
      oled.setCursor(x, 35);
      oled.print(valueStr);
      oled.setTextColor(SH110X_WHITE);
    }
  }

  if (hint && strlen(hint) > 0)
  {
    oled.setCursor(0, 44);
    oled.print(hint);
  }

  // Divider and Footer
  oled.drawLine(0, 53, 127, 53, SH110X_WHITE);
  oled.setCursor(0, 55);
  oled.print(footer ? footer : STR_FOOTER_EDIT);

  oled.display();
#endif
}

void displayUpdateConfirmation(const char *title, const char *line1, const char *line2, const char *footer)
{
#if USE_DISPLAY
  if (!connected)
    return;
  drawHeader();

  oled.setTextSize(1);
  oled.setTextColor(SH110X_WHITE);

  oled.setCursor(0, 13);
  oled.print(title ? title : "BESTAETIGUNG");

  oled.setCursor(4, 25);
  oled.print(line1 ? line1 : "");

  oled.setCursor(4, 37);
  oled.print(line2 ? line2 : "");

  // Divider and Footer
  oled.drawLine(0, 53, 127, 53, SH110X_WHITE);
  oled.setCursor(0, 55);
  oled.print(footer ? footer : STR_FOOTER_CONFIRM);

  oled.display();
#endif
}

void displayUpdateSensorMatrix(const bool hitTargets[5], int lastHitIndex)
{
#if USE_DISPLAY
  if (!connected)
    return;
  drawHeader();

  oled.setTextSize(1);
  oled.setTextColor(SH110X_WHITE);

  oled.setCursor(0, 13);
  oled.print("SENSOR LIVE-MATRIX");

  // Draw 5 Target boxes: 1, 2, 3, 4, STOP
  // Targets 1..4 are indices 1..4 in hitTargets; STOP is index 0 in hitTargets.
  const char *labels[5] = {"1", "2", "3", "4", "ST"};
  const int targetMap[5] = {1, 2, 3, 4, 0};

  for (int i = 0; i < 5; i++)
  {
    int boxX = 4 + i * 25;
    int boxY = 23;
    int tIdx = targetMap[i];
    bool isHit = hitTargets && hitTargets[tIdx];

    if (isHit)
    {
      oled.fillRect(boxX, boxY, 21, 15, SH110X_WHITE);
      oled.setTextColor(SH110X_BLACK, SH110X_WHITE);
      oled.setCursor(boxX + (i == 4 ? 4 : 7), boxY + 4);
      oled.print(labels[i]);
      oled.setTextColor(SH110X_WHITE);
    }
    else
    {
      oled.drawRect(boxX, boxY, 21, 15, SH110X_WHITE);
      oled.setCursor(boxX + (i == 4 ? 4 : 7), boxY + 4);
      oled.print(labels[i]);
    }
  }

  // Last hit detail text at y = 42
  oled.setCursor(0, 42);
  if (lastHitIndex < 0)
  {
    oled.print("Ziele anklopfen...");
  }
  else if (lastHitIndex == 0)
  {
    oled.print("Letzter: STOP-PLATE");
  }
  else
  {
    oled.printf("Letzter: ZIEL %d", lastHitIndex);
  }

  // Divider and Footer
  oled.drawLine(0, 53, 127, 53, SH110X_WHITE);
  oled.setCursor(0, 55);
  oled.print(STR_FOOTER_MATRIX);

  oled.display();
#endif
}

void displayUpdateDeviceInfo(const char *fwVersion, const char *deviceName, const char *ip, const char *mac, unsigned long uptimeSec)
{
#if USE_DISPLAY
  if (!connected)
    return;
  drawHeader();

  oled.setTextSize(1);
  oled.setTextColor(SH110X_WHITE);

  oled.setCursor(0, 13);
  oled.print("SYSTEM-INFO");

  oled.setCursor(0, 23);
  char buf[24];
  snprintf(buf, sizeof(buf), "FW: %.16s", fwVersion ? fwVersion : "v1.0");
  oled.print(buf);

  oled.setCursor(0, 33);
  snprintf(buf, sizeof(buf), "IP: %.16s", (ip && strlen(ip) > 0) ? ip : "Offline");
  oled.print(buf);

  unsigned long mins = uptimeSec / 60;
  unsigned long secs = uptimeSec % 60;
  oled.setCursor(0, 43);
  snprintf(buf, sizeof(buf), "Up: %lum %02lus  ID: %.6s", mins, secs, deviceName ? deviceName : "-");
  oled.print(buf);

  // Divider and Footer
  oled.drawLine(0, 53, 127, 53, SH110X_WHITE);
  oled.setCursor(0, 55);
  oled.print(STR_FOOTER_BACK);

  oled.display();
#endif
}

#if USE_DISPLAY
static int ssX = 10;
static int ssY = 10;
static int ssDx = 1;
static int ssDy = 1;
static unsigned long ssLastFrameMs = 0;
#endif

void displayScreensaverTick(unsigned long currentMillis, const char *deviceName, bool isConnected)
{
#if USE_DISPLAY
  if (!connected)
    return;

  // Frame limiter: ~20 FPS (every 50ms)
  if (currentMillis - ssLastFrameMs < 50)
    return;
  ssLastFrameMs = currentMillis;

  oled.clearDisplay();
  oled.setContrast(0x01); // Dim OLED brightness to protect pixels

  constexpr int BADGE_W = 58;
  constexpr int BADGE_H = 16;

  ssX += ssDx;
  ssY += ssDy;

  if (ssX <= 0)
  {
    ssX = 0;
    ssDx = 1;
  }
  else if (ssX + BADGE_W >= 127)
  {
    ssX = 127 - BADGE_W;
    ssDx = -1;
  }

  if (ssY <= 0)
  {
    ssY = 0;
    ssDy = 1;
  }
  else if (ssY + BADGE_H >= 63)
  {
    ssY = 63 - BADGE_H;
    ssDy = -1;
  }

  // Draw bouncing plate badge with rounded corners
  oled.drawRoundRect(ssX, ssY, BADGE_W, BADGE_H, 4, SH110X_WHITE);
  oled.setTextSize(1);
  oled.setTextColor(SH110X_WHITE);
  oled.setCursor(ssX + 4, ssY + 4);
  oled.print("EasySteel");

  // Subtle corner status dots (connection state)
  if (isConnected)
  {
    oled.drawPixel(127, 0, SH110X_WHITE);
  }

  oled.display();
#endif
}

void displayExitScreensaver()
{
#if USE_DISPLAY
  if (!connected)
    return;
  oled.setContrast(0x80); // Restore full brightness
#endif
}
