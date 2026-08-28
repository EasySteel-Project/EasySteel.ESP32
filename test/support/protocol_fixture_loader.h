#pragma once

#include <ArduinoJson.h>
#include <unity.h>
#include <fstream>
#include <sstream>
#include <string>

inline std::string protocolFixturePath(const char *relativePath)
{
    return std::string(FIXTURE_DIR) + relativePath;
}

inline std::string loadProtocolFixture(const char *relativePath)
{
    const std::string path = protocolFixturePath(relativePath);
    std::ifstream file(path, std::ios::binary);
    TEST_ASSERT_TRUE_MESSAGE(
        file.is_open(),
        ("Failed to open fixture: " + path).c_str());

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

inline JsonDocument loadProtocolFixtureManifest()
{
    const std::string json = loadProtocolFixture("manifest.json");
    JsonDocument document;
    const DeserializationError error = deserializeJson(document, json);
    TEST_ASSERT_TRUE_MESSAGE(
        error == DeserializationError::Ok,
        error.c_str());
    TEST_ASSERT_TRUE_MESSAGE(
        document.is<JsonArray>(),
        "Protocol fixture manifest must contain an array.");
    TEST_ASSERT_GREATER_THAN_MESSAGE(
        0,
        document.size(),
        "Protocol fixture manifest must not be empty.");
    return document;
}

inline bool protocolFixtureDirectionMatches(
    JsonObjectConst entry,
    const char *direction)
{
    const char *fixtureDirection = entry["direction"] | "";
    return strcmp(fixtureDirection, direction) == 0 ||
        strcmp(fixtureDirection, "bidirectional") == 0;
}

inline bool protocolFixtureLayerMatches(
    JsonObjectConst entry,
    const char *firstLayer,
    const char *secondLayer = nullptr)
{
    const char *layer = entry["layer"] | "";
    return strcmp(layer, firstLayer) == 0 ||
        (secondLayer && strcmp(layer, secondLayer) == 0);
}

inline const char *protocolFixtureId(JsonObjectConst entry)
{
    return entry["id"] | "<missing-fixture-id>";
}
