#include <unity.h>
#include <ArduinoJson.h>
#include "Arduino.h"
#include "WiFiClient.h"
#include "mock_globals.cpp"
#include "tcp_protocol.h"
#include "tcp_protocol.cpp"

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

void setUp(void) {}
void tearDown(void) {}

void test_tcp_json_hit_event_format(void)
{
    std::string jsonStr = buildJsonEvent(
        "ESP32_A1B2C3", "Target_1", "hit", 2, 1450, 5);

    TEST_ASSERT_TRUE(jsonStr.find("\"deviceId\":\"ESP32_A1B2C3\"") != std::string::npos);
    TEST_ASSERT_TRUE(jsonStr.find("\"deviceName\":\"Target_1\"") != std::string::npos);
    TEST_ASSERT_TRUE(jsonStr.find("\"event\":\"hit\"") != std::string::npos);
    TEST_ASSERT_TRUE(jsonStr.find("\"target\":2") != std::string::npos);
    TEST_ASSERT_TRUE(jsonStr.find("\"timeMs\":1450") != std::string::npos);
    TEST_ASSERT_TRUE(jsonStr.find("\"targetCount\":5") != std::string::npos);
    TEST_ASSERT_TRUE(jsonStr.find("\"protocolVersion\":1") != std::string::npos);
    TEST_ASSERT_EQUAL('\n', jsonStr.back());
}

void test_tcp_json_end_event_has_auditable_result_fields(void)
{
    std::string jsonStr = buildJsonEvent(
        "ESP32_A1B2C3",
        "Target_1",
        "end",
        -1,
        7250,
        5,
        "abcd-1",
        1250,
        1,
        2,
        "match-17:stage-3");

    TEST_ASSERT_TRUE(jsonStr.find("\"event\":\"end\"") != std::string::npos);
    TEST_ASSERT_TRUE(jsonStr.find("\"runId\":\"abcd-1\"") != std::string::npos);
    TEST_ASSERT_TRUE(jsonStr.find("\"rawTimeMs\":1250") != std::string::npos);
    TEST_ASSERT_TRUE(jsonStr.find("\"missCount\":1") != std::string::npos);
    TEST_ASSERT_TRUE(jsonStr.find("\"penaltyCount\":2") != std::string::npos);
    TEST_ASSERT_TRUE(jsonStr.find("\"penaltyTimeMs\":6000") != std::string::npos);
    TEST_ASSERT_TRUE(jsonStr.find("\"contextId\":\"match-17:stage-3\"") != std::string::npos);
    TEST_ASSERT_TRUE(jsonStr.find("\"target\":") == std::string::npos);
    TEST_ASSERT_EQUAL('\n', jsonStr.back());
}

void test_tcp_json_ping_event_has_no_result_fields(void)
{
    std::string jsonStr = buildJsonEvent(
        "ESP32_A1B2C3", "Target_1", "ping", -1, 0, 5);

    TEST_ASSERT_TRUE(jsonStr.find("\"event\":\"ping\"") != std::string::npos);
    TEST_ASSERT_TRUE(jsonStr.find("\"penaltyCount\":") == std::string::npos);
    TEST_ASSERT_TRUE(jsonStr.find("\"runId\":") == std::string::npos);
    TEST_ASSERT_EQUAL('\n', jsonStr.back());
}

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

void test_tcp_serialization_accepts_maximum_valid_result_fields(void)
{
    String deviceId(std::string(31, 'a'));
    String deviceName(std::string(31, 'b'));
    std::string runId(23, 'c');
    std::string contextId(47, 'd');
    char buffer[TCP_EVENT_MAX_BYTES] = {};

    size_t length = tcpProtocolSerializeEvent(
        buffer,
        sizeof(buffer),
        deviceId,
        deviceName,
        "end",
        -1,
        30000,
        5,
        runId.c_str(),
        0,
        4,
        10,
        contextId.c_str());

    TEST_ASSERT_GREATER_THAN_UINT32(0, length);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(TCP_EVENT_MAX_BYTES - 1, length);
}

void test_tcp_serialization_rejects_invalid_utf8_device_name(void)
{
    const char invalidName[] = {static_cast<char>(0xc3), '(', '\0'};
    char buffer[TCP_EVENT_MAX_BYTES] = {};

    TEST_ASSERT_EQUAL_UINT32(
        0,
        tcpProtocolSerializeEvent(
            buffer,
            sizeof(buffer),
            "ESP32_A1B2C3",
            invalidName,
            "online",
            -1,
            0,
            5));
}

void test_parse_capabilities_message(void)
{
    const char *json =
        "{\"protocolVersion\":1,\"event\":\"capabilities\",\"resultAck\":true}";
    TcpServerMessage message = {};

    TEST_ASSERT_TRUE(tcpProtocolParseServerMessage(json, strlen(json), message));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TcpServerMessageType::Capabilities),
        static_cast<int>(message.type));
}

void test_reject_capabilities_without_required_result_ack(void)
{
    const char *json =
        "{\"protocolVersion\":1,\"event\":\"capabilities\",\"resultAck\":false}";
    TcpServerMessage message = {};

    TEST_ASSERT_FALSE(tcpProtocolParseServerMessage(json, strlen(json), message));
}

void test_parse_ack_message_uses_canonical_envelope(void)
{
    const char *json =
        "{\"protocolVersion\":1,\"event\":\"ack\",\"runId\":\"91af20c3-7\"}";
    TcpServerMessage message = {};

    TEST_ASSERT_TRUE(tcpProtocolParseServerMessage(json, strlen(json), message));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TcpServerMessageType::Acknowledgement),
        static_cast<int>(message.type));
    TEST_ASSERT_EQUAL_STRING("91af20c3-7", message.runId);
}

void test_parse_match_info_message(void)
{
    const char *json =
        "{\"protocolVersion\":1,\"event\":\"match_info\","
        "\"contextId\":\"match-17:stage-3\",\"stage\":\"Roundabout\"}";
    TcpServerMessage message = {};

    TEST_ASSERT_TRUE(tcpProtocolParseServerMessage(json, strlen(json), message));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TcpServerMessageType::MatchInfo),
        static_cast<int>(message.type));
    TEST_ASSERT_EQUAL_STRING("match-17:stage-3", message.contextId);
    TEST_ASSERT_EQUAL_STRING("Roundabout", message.stage);
}

void test_parse_rejects_wrong_version_and_legacy_ack(void)
{
    const char *wrongVersion =
        "{\"protocolVersion\":2,\"event\":\"ack\",\"runId\":\"run-1\"}";
    const char *legacyAck = "{\"ack\":\"run-1\"}";
    TcpServerMessage message = {};

    TEST_ASSERT_FALSE(tcpProtocolParseServerMessage(
        wrongVersion, strlen(wrongVersion), message));
    TEST_ASSERT_FALSE(tcpProtocolParseServerMessage(
        legacyAck, strlen(legacyAck), message));
}

void test_parse_rejects_unsafe_match_text(void)
{
    const char *json =
        "{\"protocolVersion\":1,\"event\":\"match_info\","
        "\"contextId\":\"match-17\",\"stage\":\"Bad\\nStage\"}";
    TcpServerMessage message = {};

    TEST_ASSERT_FALSE(tcpProtocolParseServerMessage(json, strlen(json), message));
}

void test_parse_rejects_duplicate_or_escaped_property_names(void)
{
    const char *duplicate =
        "{\"protocolVersion\":1,\"event\":\"ack\",\"event\":\"capabilities\","
        "\"runId\":\"run-1\"}";
    const char *escapedProperty =
        "{\"protocolVersion\":1,\"ev\\u0065nt\":\"ack\",\"runId\":\"run-1\"}";
    TcpServerMessage message = {};

    TEST_ASSERT_FALSE(tcpProtocolParseServerMessage(
        duplicate, strlen(duplicate), message));
    TEST_ASSERT_FALSE(tcpProtocolParseServerMessage(
        escapedProperty, strlen(escapedProperty), message));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_tcp_json_hit_event_format);
    RUN_TEST(test_tcp_json_end_event_has_auditable_result_fields);
    RUN_TEST(test_tcp_json_ping_event_has_no_result_fields);
    RUN_TEST(test_tcp_serialization_rejects_invalid_result);
    RUN_TEST(test_tcp_serialization_rejects_too_small_buffer);
    RUN_TEST(test_tcp_serialization_rejects_document_overflow);
    RUN_TEST(test_tcp_serialization_accepts_maximum_valid_result_fields);
    RUN_TEST(test_tcp_serialization_rejects_invalid_utf8_device_name);
    RUN_TEST(test_parse_capabilities_message);
    RUN_TEST(test_reject_capabilities_without_required_result_ack);
    RUN_TEST(test_parse_ack_message_uses_canonical_envelope);
    RUN_TEST(test_parse_match_info_message);
    RUN_TEST(test_parse_rejects_wrong_version_and_legacy_ack);
    RUN_TEST(test_parse_rejects_unsafe_match_text);
    RUN_TEST(test_parse_rejects_duplicate_or_escaped_property_names);
    return UNITY_END();
}
