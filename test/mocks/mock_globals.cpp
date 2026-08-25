#include "Arduino.h"

unsigned long g_mockMillis = 0;
unsigned long g_mockMicros = 0;
int g_mockPinState = 1; // HIGH
MockSerial Serial;
