#include "tcp_protocol.h"
#include <ArduinoJson.h>
#include <cctype>
#include <cstring>

static bool isValidUtf8(const char *value, size_t length)
{
  size_t index = 0;
  while (index < length)
  {
    const unsigned char first = static_cast<unsigned char>(value[index]);
    if (first <= 0x7f)
    {
      index++;
      continue;
    }

    size_t continuationCount = 0;
    unsigned long codePoint = 0;
    if (first >= 0xc2 && first <= 0xdf)
    {
      continuationCount = 1;
      codePoint = first & 0x1f;
    }
    else if (first >= 0xe0 && first <= 0xef)
    {
      continuationCount = 2;
      codePoint = first & 0x0f;
    }
    else if (first >= 0xf0 && first <= 0xf4)
    {
      continuationCount = 3;
      codePoint = first & 0x07;
    }
    else
    {
      return false;
    }

    if (index + continuationCount >= length)
    {
      return false;
    }

    for (size_t continuation = 1; continuation <= continuationCount; continuation++)
    {
      const unsigned char next = static_cast<unsigned char>(value[index + continuation]);
      if ((next & 0xc0) != 0x80)
      {
        return false;
      }
      codePoint = (codePoint << 6) | (next & 0x3f);
    }

    if ((continuationCount == 2 && codePoint < 0x800) ||
        (continuationCount == 3 && codePoint < 0x10000) ||
        (codePoint >= 0xd800 && codePoint <= 0xdfff) ||
        codePoint > 0x10ffff)
    {
      return false;
    }

    index += continuationCount + 1;
  }

  return true;
}

static bool isPrintableText(const char *value, size_t capacity, bool allowEmpty)
{
  if (!value)
  {
    return false;
  }

  const size_t length = strlen(value);
  if ((!allowEmpty && length == 0) || length >= capacity ||
      !isValidUtf8(value, length))
  {
    return false;
  }

  for (size_t index = 0; index < length; index++)
  {
    const unsigned char character = static_cast<unsigned char>(value[index]);
    if (character < 0x20 || character == 0x7f)
    {
      return false;
    }
  }

  return true;
}

static bool isPrintableAscii(const char *value, size_t capacity, bool allowEmpty)
{
  if (!isPrintableText(value, capacity, allowEmpty))
  {
    return false;
  }

  for (size_t index = 0; value[index] != '\0'; index++)
  {
    if (static_cast<unsigned char>(value[index]) > 0x7e)
    {
      return false;
    }
  }

  return true;
}

static bool isIdentifier(const char *value, size_t capacity, bool allowEmpty)
{
  if (!isPrintableAscii(value, capacity, allowEmpty))
  {
    return false;
  }

  for (size_t index = 0; value[index] != '\0'; index++)
  {
    const unsigned char character = static_cast<unsigned char>(value[index]);
    if (!std::isalnum(character) && character != '-' && character != '_' &&
        character != '.' && character != ':')
    {
      return false;
    }
  }

  return true;
}

static bool isKnownDeviceEvent(const char *event)
{
  return strcmp(event, "online") == 0 ||
    strcmp(event, "start_pressed") == 0 ||
    strcmp(event, "start") == 0 ||
    strcmp(event, "hit") == 0 ||
    strcmp(event, "end") == 0 ||
    strcmp(event, "reset") == 0 ||
    strcmp(event, "settings_changed") == 0 ||
    strcmp(event, "ping") == 0;
}

static bool hasValidUniqueTopLevelProperties(const char *json, size_t length)
{
  static constexpr size_t MAX_PROPERTIES = 24;
  static constexpr size_t MAX_PROPERTY_BYTES = 31;
  char propertyNames[MAX_PROPERTIES][MAX_PROPERTY_BYTES + 1] = {};
  size_t propertyCount = 0;
  int depth = 0;
  bool inString = false;
  bool escaped = false;
  bool expectingProperty = false;
  bool readingProperty = false;
  char propertyName[MAX_PROPERTY_BYTES + 1] = {};
  size_t propertyLength = 0;

  for (size_t index = 0; index < length; index++)
  {
    const char character = json[index];
    if (inString)
    {
      if (escaped)
      {
        if (readingProperty)
        {
          return false;
        }
        escaped = false;
        continue;
      }
      if (character == '\\')
      {
        escaped = true;
        continue;
      }
      if (character == '"')
      {
        inString = false;
        if (readingProperty)
        {
          propertyName[propertyLength] = '\0';
          if (!isIdentifier(propertyName, sizeof(propertyName), false) ||
              propertyCount >= MAX_PROPERTIES)
          {
            return false;
          }
          for (size_t propertyIndex = 0; propertyIndex < propertyCount; propertyIndex++)
          {
            if (strcmp(propertyNames[propertyIndex], propertyName) == 0)
            {
              return false;
            }
          }
          snprintf(propertyNames[propertyCount], sizeof(propertyNames[propertyCount]), "%s", propertyName);
          propertyCount++;
          readingProperty = false;
          expectingProperty = false;
        }
        continue;
      }
      if (readingProperty)
      {
        if (propertyLength >= MAX_PROPERTY_BYTES)
        {
          return false;
        }
        propertyName[propertyLength++] = character;
      }
      continue;
    }

    if (character == '"')
    {
      inString = true;
      if (depth == 1 && expectingProperty)
      {
        readingProperty = true;
        propertyLength = 0;
      }
    }
    else if (character == '{' || character == '[')
    {
      depth++;
      if (depth == 1 && character == '{')
      {
        expectingProperty = true;
      }
    }
    else if (character == '}' || character == ']')
    {
      depth--;
    }
    else if (character == ',' && depth == 1)
    {
      expectingProperty = true;
    }
  }

  return !inString && !readingProperty && propertyCount > 0;
}

size_t tcpProtocolSerializeEvent(
  char *buffer,
  size_t capacity,
  const String &chipId,
  const String &deviceName,
  const char *event,
  int target,
  unsigned long timeMs,
  int targetCount,
  const char *runId,
  unsigned long rawTimeMs,
  int missCount,
  int penaltyCount,
  const char *contextId)
{
  if (!buffer || capacity < 2 || !event ||
      !isIdentifier(chipId.c_str(), 32, false) ||
      !isPrintableText(deviceName.c_str(), 32, false) ||
      !isIdentifier(event, 32, false) ||
      !isKnownDeviceEvent(event) ||
      targetCount < 1 || targetCount > TCP_MAX_TARGET_COUNT ||
      timeMs > TCP_MAX_ROUND_TIME_MS)
  {
    return 0;
  }

  const bool isEndEvent = strcmp(event, "end") == 0;
  const bool isHitEvent = strcmp(event, "hit") == 0;
  if ((isHitEvent && (target < 0 || target >= targetCount)) ||
      (!isHitEvent && target >= 0) ||
      (!isHitEvent && !isEndEvent && timeMs != 0))
  {
    return 0;
  }

  // ArduinoJson 7 deprecates the fixed-capacity alias, but it deliberately avoids heap use here.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  StaticJsonDocument<TCP_EVENT_MAX_BYTES> doc;
#pragma GCC diagnostic pop
  doc["protocolVersion"] = TCP_PROTOCOL_VERSION;
  doc["deviceId"] = chipId;
  doc["deviceName"] = deviceName;
  doc["event"] = event;
  if (target >= 0)
  {
    doc["target"] = target;
  }
  doc["timeMs"] = timeMs;
  doc["targetCount"] = targetCount;

  if (isEndEvent)
  {
    if (!isIdentifier(runId, TCP_RUN_ID_BYTES, false) ||
        missCount < 0 || missCount > targetCount - 1 ||
        penaltyCount < missCount || penaltyCount > TCP_MAX_PENALTY_COUNT ||
        rawTimeMs > timeMs ||
        timeMs != (rawTimeMs + static_cast<unsigned long>(penaltyCount) * 3000UL > TCP_MAX_ROUND_TIME_MS
          ? TCP_MAX_ROUND_TIME_MS
          : rawTimeMs + static_cast<unsigned long>(penaltyCount) * 3000UL) ||
        (contextId && !isIdentifier(contextId, TCP_CONTEXT_ID_BYTES, true)))
    {
      return 0;
    }

    doc["runId"] = runId;
    doc["rawTimeMs"] = rawTimeMs;
    doc["missCount"] = missCount;
    doc["penaltyCount"] = penaltyCount;
    doc["penaltyTimeMs"] = timeMs - rawTimeMs;
    if (contextId && contextId[0] != '\0')
    {
      doc["contextId"] = contextId;
    }
  }
  else if ((runId && runId[0] != '\0') || rawTimeMs != 0 ||
           missCount != 0 || penaltyCount != 0 ||
           (contextId && contextId[0] != '\0'))
  {
    return 0;
  }

  if (doc.overflowed())
  {
    return 0;
  }

  const size_t jsonLength = measureJson(doc);
  if (jsonLength > TCP_JSON_MAX_BYTES || jsonLength + 2 > capacity)
  {
    return 0;
  }

  serializeJson(doc, buffer, capacity);
  buffer[jsonLength] = '\n';
  buffer[jsonLength + 1] = '\0';
  return jsonLength + 1;
}

bool tcpProtocolParseServerMessage(
  const char *json,
  size_t length,
  TcpServerMessage& message)
{
  message = {};
  message.type = TcpServerMessageType::Invalid;
  if (!json || length == 0 || length > TCP_JSON_MAX_BYTES ||
      !hasValidUniqueTopLevelProperties(json, length))
  {
    return false;
  }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  StaticJsonDocument<1024> doc;
#pragma GCC diagnostic pop
  const DeserializationError error = deserializeJson(doc, json, length);
  if (error != DeserializationError::Ok || doc.overflowed() || !doc.is<JsonObject>())
  {
    return false;
  }

  if (!doc["protocolVersion"].is<int>() ||
      doc["protocolVersion"].as<int>() != TCP_PROTOCOL_VERSION ||
      !doc["event"].is<const char *>())
  {
    return false;
  }

  const char *event = doc["event"].as<const char *>();
  if (!isIdentifier(event, 32, false))
  {
    return false;
  }

  if (strcmp(event, "capabilities") == 0)
  {
    if (!doc["resultAck"].is<bool>() || !doc["resultAck"].as<bool>())
    {
      return false;
    }
    message.type = TcpServerMessageType::Capabilities;
    return true;
  }

  if (strcmp(event, "ack") == 0)
  {
    if (!doc["runId"].is<const char *>())
    {
      return false;
    }
    const char *runId = doc["runId"].as<const char *>();
    if (!isIdentifier(runId, sizeof(message.runId), false))
    {
      return false;
    }
    snprintf(message.runId, sizeof(message.runId), "%s", runId);
    message.type = TcpServerMessageType::Acknowledgement;
    return true;
  }

  if (strcmp(event, "match_info") == 0)
  {
    if (!doc["contextId"].is<const char *>() || !doc["stage"].is<const char *>())
    {
      return false;
    }
    const char *contextId = doc["contextId"].as<const char *>();
    const char *stage = doc["stage"].as<const char *>();
    const bool clearContext = contextId[0] == '\0' && stage[0] == '\0';
    if ((!clearContext &&
         (!isIdentifier(contextId, sizeof(message.contextId), false) ||
          !isPrintableAscii(stage, sizeof(message.stage), false))) ||
        (clearContext &&
         (!isIdentifier(contextId, sizeof(message.contextId), true) ||
          !isPrintableAscii(stage, sizeof(message.stage), true))))
    {
      return false;
    }
    snprintf(message.contextId, sizeof(message.contextId), "%s", contextId);
    snprintf(message.stage, sizeof(message.stage), "%s", stage);
    message.type = TcpServerMessageType::MatchInfo;
    return true;
  }

  message.type = TcpServerMessageType::Unknown;
  return true;
}

void tcpProtocolWriteEvent(
  WiFiClient &client,
  const String &chipId,
  const String &deviceName,
  const char *event,
  int target,
  unsigned long timeMs,
  int targetCount,
  const char *runId,
  unsigned long rawTimeMs,
  int missCount,
  int penaltyCount,
  const char *contextId)
{
  char buffer[TCP_EVENT_MAX_BYTES] = {};
  const size_t length = tcpProtocolSerializeEvent(
    buffer, sizeof(buffer), chipId, deviceName, event, target, timeMs,
    targetCount, runId, rawTimeMs, missCount, penaltyCount, contextId);
  if (length > 0)
  {
    client.write(reinterpret_cast<const uint8_t *>(buffer), length);
  }
}
