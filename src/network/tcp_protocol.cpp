#include "tcp_protocol.h"
#include <ArduinoJson.h>

size_t tcpProtocolSerializeEvent(
  char *buffer,
  size_t capacity,
  const String &chipId,
  const String &deviceName,
  const char *event,
  int target,
  unsigned long timeMs,
  int targetCount,
  int missedTargets,
  const char *runId)
{
  if (!buffer || capacity == 0)
    return 0;

  // ArduinoJson 7 deprecates the fixed-capacity alias, but it deliberately avoids heap use here.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  StaticJsonDocument<384> doc;
#pragma GCC diagnostic pop
  doc["protocolVersion"] = TCP_PROTOCOL_VERSION;
  doc["deviceId"] = chipId;
  doc["deviceName"] = deviceName;
  doc["event"] = event;
  if (target >= 0)
    doc["target"] = target;
  doc["time"] = timeMs;
  doc["targetCount"] = targetCount;

  if (runId && runId[0] != '\0')
    doc["runId"] = runId;

  if (strcmp(event, "end") == 0 || missedTargets > 0)
    doc["missedTargets"] = missedTargets;

  if (doc.overflowed())
    return 0;

  const size_t jsonLength = measureJson(doc);
  if (jsonLength + 2 > capacity)
    return 0;

  serializeJson(doc, buffer, capacity);
  buffer[jsonLength] = '\n';
  buffer[jsonLength + 1] = '\0';
  return jsonLength + 1;
}

void tcpProtocolWriteEvent(
  WiFiClient &client,
  const String &chipId,
  const String &deviceName,
  const char *event,
  int target,
  unsigned long timeMs,
  int targetCount,
  int missedTargets,
  const char *runId)
{
  char buffer[TCP_EVENT_MAX_BYTES] = {};
  const size_t length = tcpProtocolSerializeEvent(
    buffer, sizeof(buffer), chipId, deviceName, event, target, timeMs,
    targetCount, missedTargets, runId);
  if (length > 0)
    client.write(reinterpret_cast<const uint8_t *>(buffer), length);
}
