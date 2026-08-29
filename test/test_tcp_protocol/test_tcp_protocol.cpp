#include <unity.h>
#include <ArduinoJson.h>
#include "Arduino.h"
#include "WiFiClient.h"
#include "mock_globals.cpp"
#include "tcp_protocol.h"
#include "tcp_protocol.cpp"
#include "../support/protocol_fixture_loader.h"
#include <string>

std::string buildJsonEvent(
    const String& chipId,
    const String& deviceName,
    const char *event,
    int target,
    unsigned long timeMs,
    int targetCount,
    const char *runId = nullptr,
    unsigned long rawTimeMs = 0,
    int missCount = 0,
    int penaltyCount = 0,
    const char *contextId = nullptr)
{
    WiFiClient client;
    tcpProtocolWriteEvent(
        client,
        chipId,
        deviceName,
        event,
        target,
        timeMs,
        targetCount,
        runId,
        rawTimeMs,
        missCount,
        penaltyCount,
        contextId);
    return client.writtenData;
}

void assertJsonEquivalence(
    const std::string& expectedJson,
    const std::string& actualJson,
    const char *fixtureId)
{
    JsonDocument expectedDocument;
    JsonDocument actualDocument;
    const DeserializationError expectedError =
        deserializeJson(expectedDocument, expectedJson);
    const DeserializationError actualError =
        deserializeJson(actualDocument, actualJson);

    TEST_ASSERT_TRUE_MESSAGE(
        expectedError == DeserializationError::Ok,
        fixtureId);
    TEST_ASSERT_TRUE_MESSAGE(
        actualError == DeserializationError::Ok,
        fixtureId);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        expectedDocument.size(),
        actualDocument.size(),
        fixtureId);

    for (JsonPairConst pair : expectedDocument.as<JsonObjectConst>())
    {
        JsonVariantConst actualValue = actualDocument[pair.key()];
        TEST_ASSERT_FALSE_MESSAGE(actualValue.isNull(), fixtureId);

        std::string expectedValue;
        std::string serializedActualValue;
        serializeJson(pair.value(), expectedValue);
        serializeJson(actualValue, serializedActualValue);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(
            expectedValue.c_str(),
            serializedActualValue.c_str(),
            fixtureId);
    }
}

std::string buildJsonEventForFixture(JsonObjectConst entry)
{
    JsonObjectConst fields = entry["expectedFields"].as<JsonObjectConst>();
    const char *event = entry["expectedEvent"].as<const char *>();
    const int target = fields["target"].is<int>()
        ? fields["target"].as<int>()
        : -1;
    const char *runId = fields["runId"].is<const char *>()
        ? fields["runId"].as<const char *>()
        : nullptr;
    const char *contextId = fields["contextId"].is<const char *>()
        ? fields["contextId"].as<const char *>()
        : nullptr;
    const uint32_t sequence = fields["sequence"].is<uint32_t>()
        ? fields["sequence"].as<uint32_t>()
        : 0;

    return buildJsonEvent(
        fields["deviceId"].as<const char *>(),
        fields["deviceName"].as<const char *>(),
        event,
        target,
        fields["timeMs"].as<unsigned long>(),
        fields["targetCount"].as<int>(),
        runId,
        fields["rawTimeMs"].is<unsigned long>()
            ? fields["rawTimeMs"].as<unsigned long>()
            : 0,
        fields["missCount"].is<int>()
            ? fields["missCount"].as<int>()
            : 0,
        fields["penaltyCount"].is<int>()
            ? fields["penaltyCount"].as<int>()
            : 0,
        contextId,
        sequence);
}

TcpServerMessageType expectedServerMessageType(const char *event)
{
    if (strcmp(event, "capabilities") == 0)
    {
        return TcpServerMessageType::Capabilities;
    }
    if (strcmp(event, "ack") == 0)
    {
        return TcpServerMessageType::Acknowledgement;
    }
    if (strcmp(event, "match_info") == 0)
    {
        return TcpServerMessageType::MatchInfo;
    }
    if (strcmp(event, "ping") == 0)
    {
        return TcpServerMessageType::Ping;
    }
    return TcpServerMessageType::Unknown;
}

void setUp(void)
{
}

void tearDown(void)
{
}

void test_device_serialization_vectors(void)
{
    JsonDocument manifest = loadProtocolFixtureManifest();
    size_t executedVectors = 0;

    for (JsonObjectConst entry : manifest.as<JsonArrayConst>())
    {
        const char *direction = entry["direction"] | "";
        if (strcmp(direction, "device-to-server") != 0 ||
            !protocolFixtureLayerMatches(
                entry,
                "serialization",
                "end-to-end"))
        {
            continue;
        }

        executedVectors++;
        const char *fixtureId = protocolFixtureId(entry);
        const std::string expected =
            loadProtocolFixture(entry["file"] | "");
        const std::string actual = buildJsonEventForFixture(entry);
        TEST_ASSERT_FALSE_MESSAGE(actual.empty(), fixtureId);
        TEST_ASSERT_EQUAL_CHAR_MESSAGE('\n', actual.back(), fixtureId);
        assertJsonEquivalence(expected, actual, fixtureId);
    }

    TEST_ASSERT_GREATER_THAN_MESSAGE(
        0,
        executedVectors,
        "No device serialization vectors were executed.");
}

void test_server_parsing_vectors(void)
{
    JsonDocument manifest = loadProtocolFixtureManifest();
    size_t executedVectors = 0;

    for (JsonObjectConst entry : manifest.as<JsonArrayConst>())
    {
        if (!protocolFixtureDirectionMatches(entry, "server-to-device") ||
            !protocolFixtureLayerMatches(entry, "parsing", "end-to-end"))
        {
            continue;
        }

        executedVectors++;
        const char *fixtureId = protocolFixtureId(entry);
        const std::string json =
            loadProtocolFixture(entry["file"] | "");
        TcpServerMessage message = {};
        const bool parsed = tcpProtocolParseServerMessage(
            json.c_str(),
            json.size(),
            message);
        const char *expectedResult = entry["expectedResult"] | "";

        if (strcmp(expectedResult, "rejected") == 0)
        {
            TEST_ASSERT_FALSE_MESSAGE(parsed, fixtureId);
            continue;
        }

        TEST_ASSERT_TRUE_MESSAGE(parsed, fixtureId);
        const char *expectedEvent = entry["expectedEvent"] | "";
        TEST_ASSERT_EQUAL_INT_MESSAGE(
            static_cast<int>(expectedServerMessageType(expectedEvent)),
            static_cast<int>(message.type),
            fixtureId);

        JsonObjectConst fields =
            entry["expectedFields"].as<JsonObjectConst>();
        if (message.type == TcpServerMessageType::Acknowledgement)
        {
            TEST_ASSERT_EQUAL_STRING_MESSAGE(
                fields["runId"] | "",
                message.runId,
                fixtureId);
        }
        else if (message.type == TcpServerMessageType::MatchInfo)
        {
            TEST_ASSERT_EQUAL_STRING_MESSAGE(
                fields["contextId"] | "",
                message.contextId,
                fixtureId);
            TEST_ASSERT_EQUAL_STRING_MESSAGE(
                fields["stage"] | "",
                message.stage,
                fixtureId);
        }
        else if (message.type == TcpServerMessageType::Ping)
        {
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(
                fields["sequence"].as<uint32_t>(),
                message.sequence,
                fixtureId);
        }
    }

    TEST_ASSERT_GREATER_THAN_MESSAGE(
        0,
        executedVectors,
        "No server parsing vectors were executed.");
}

// These local safety tests exercise implementation details, not shared wire data.
void test_tcp_serialization_rejects_invalid_result(void)
{
    char buffer[TCP_EVENT_MAX_BYTES] = {};
    TEST_ASSERT_EQUAL_UINT32(
        0,
        tcpProtocolSerializeEvent(
            buffer,
            sizeof(buffer),
            "ESP32_A1B2C3",
            "Target_1",
            "end",
            -1,
            4250,
            5,
            "",
            1250,
            1,
            1));
}

void test_tcp_serialization_rejects_too_small_buffer(void)
{
    char buffer[16] = {};
    TEST_ASSERT_EQUAL_UINT32(
        0,
        tcpProtocolSerializeEvent(
            buffer,
            sizeof(buffer),
            "ESP32_A1B2C3",
            "Target_1",
            "hit",
            2,
            1450,
            5));
}

void test_tcp_serialization_rejects_document_overflow(void)
{
    String oversizedName;
    for (int index = 0; index < 500; index++)
    {
        oversizedName += 'x';
    }

    char buffer[TCP_EVENT_MAX_BYTES] = {};
    TEST_ASSERT_EQUAL_UINT32(
        0,
        tcpProtocolSerializeEvent(
            buffer,
            sizeof(buffer),
            "ESP32_A1B2C3",
            oversizedName,
            "hit",
            2,
            1450,
            5));
}

void test_result_run_id_accepts_32_hex_bytes_and_rejects_33(void)
{
    char buffer[TCP_EVENT_MAX_BYTES] = {};
    const char *validRunId = "0123456789abcdef0123456789abcdef";
    TEST_ASSERT_GREATER_THAN_UINT32(
        0,
        tcpProtocolSerializeEvent(
            buffer,
            sizeof(buffer),
            "ESP32_A1B2C3",
            "Target_1",
            "end",
            -1,
            1250,
            5,
            validRunId,
            1250,
            0,
            0));

    TEST_ASSERT_EQUAL_UINT32(
        0,
        tcpProtocolSerializeEvent(
            buffer,
            sizeof(buffer),
            "ESP32_A1B2C3",
            "Target_1",
            "end",
            -1,
            1250,
            5,
            "0123456789abcdef0123456789abcdef0",
            1250,
            0,
            0));
}

void test_parse_rejects_escaped_property_names(void)
{
    const char *json =
        "{\"protocolVersion\":1,\"ev\\u0065nt\":\"ack\","
        "\"runId\":\"run-1\"}";
    TcpServerMessage message = {};

    TEST_ASSERT_FALSE(tcpProtocolParseServerMessage(
        json,
        strlen(json),
        message));
}

void test_match_info_ignores_deferred_v2_display_fields(void)
{
    const char *json =
        "{\"protocolVersion\":1,\"event\":\"match_info\","
        "\"contextId\":\"match-17:stage-3\",\"stage\":\"Roundabout\","
        "\"competitorName\":\"Pilot Shooter\",\"stringNumber\":3}";
    TcpServerMessage message = {};

    TEST_ASSERT_TRUE(tcpProtocolParseServerMessage(
        json,
        strlen(json),
        message));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TcpServerMessageType::MatchInfo),
        static_cast<int>(message.type));
    TEST_ASSERT_EQUAL_STRING("match-17:stage-3", message.contextId);
    TEST_ASSERT_EQUAL_STRING("Roundabout", message.stage);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_device_serialization_vectors);
    RUN_TEST(test_server_parsing_vectors);
    RUN_TEST(test_tcp_serialization_rejects_invalid_result);
    RUN_TEST(test_tcp_serialization_rejects_too_small_buffer);
    RUN_TEST(test_tcp_serialization_rejects_document_overflow);
    RUN_TEST(test_result_run_id_accepts_32_hex_bytes_and_rejects_33);
    RUN_TEST(test_parse_rejects_escaped_property_names);
    RUN_TEST(test_match_info_ignores_deferred_v2_display_fields);
    return UNITY_END();
}
