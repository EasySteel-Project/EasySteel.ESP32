#include <unity.h>
#include <ArduinoJson.h>
#include "Arduino.h"
#include "WiFiClient.h"
#include "mock_globals.cpp"
#include "tcp_protocol.h"
#include "tcp_protocol.cpp"

std::string buildJsonEvent(const String& chipId, const String& deviceName, const char *event, int target, unsigned long timeMs, int targetCount, int missedTargets, const char *runId = nullptr) {
    WiFiClient client;
    tcpProtocolWriteEvent(client, chipId, deviceName, event, target, timeMs,
                          targetCount, missedTargets, runId);
    return client.writtenData;
}

void setUp(void) {}
void tearDown(void) {}

void test_tcp_json_hit_event_format(void) {
    std::string jsonStr = buildJsonEvent("ESP32_A1B2C3", "Target_1", "hit", 2, 1450, 5, 0);

    TEST_ASSERT_TRUE(jsonStr.find("\"deviceId\":\"ESP32_A1B2C3\"") != std::string::npos);
    TEST_ASSERT_TRUE(jsonStr.find("\"deviceName\":\"Target_1\"") != std::string::npos);
    TEST_ASSERT_TRUE(jsonStr.find("\"event\":\"hit\"") != std::string::npos);
    TEST_ASSERT_TRUE(jsonStr.find("\"target\":2") != std::string::npos);
    TEST_ASSERT_TRUE(jsonStr.find("\"time\":1450") != std::string::npos);
    TEST_ASSERT_TRUE(jsonStr.find("\"targetCount\":5") != std::string::npos);
    TEST_ASSERT_TRUE(jsonStr.find("\"protocolVersion\":1") != std::string::npos);
    // Ends with newline
    TEST_ASSERT_EQUAL('\n', jsonStr.back());
}

void test_tcp_json_end_event_includes_missed_targets(void) {
    std::string jsonStr = buildJsonEvent("ESP32_A1B2C3", "Target_1", "end", -1, 4500, 5, 2, "abcd-1");

    TEST_ASSERT_TRUE(jsonStr.find("\"event\":\"end\"") != std::string::npos);
    TEST_ASSERT_TRUE(jsonStr.find("\"missedTargets\":2") != std::string::npos);
    TEST_ASSERT_TRUE(jsonStr.find("\"runId\":\"abcd-1\"") != std::string::npos);
    // target should not be present when < 0
    TEST_ASSERT_TRUE(jsonStr.find("\"target\":") == std::string::npos);
    TEST_ASSERT_EQUAL('\n', jsonStr.back());
}

void test_tcp_json_ping_event(void) {
    std::string jsonStr = buildJsonEvent("ESP32_A1B2C3", "Target_1", "ping", -1, 0, 5, 0);

    TEST_ASSERT_TRUE(jsonStr.find("\"event\":\"ping\"") != std::string::npos);
    TEST_ASSERT_TRUE(jsonStr.find("\"missedTargets\":") == std::string::npos);
    TEST_ASSERT_EQUAL('\n', jsonStr.back());
}

void test_tcp_serialization_rejects_too_small_buffer(void) {
    char buffer[16] = {};
    TEST_ASSERT_EQUAL_UINT32(
      0,
      tcpProtocolSerializeEvent(
        buffer, sizeof(buffer), "ESP32_A1B2C3", "Target_1", "hit",
        2, 1450, 5, 0));
}

void test_tcp_serialization_rejects_document_overflow(void) {
    String oversizedName;
    for (int index = 0; index < 500; index++)
        oversizedName += 'x';

    char buffer[TCP_EVENT_MAX_BYTES] = {};
    TEST_ASSERT_EQUAL_UINT32(
      0,
      tcpProtocolSerializeEvent(
        buffer, sizeof(buffer), "ESP32_A1B2C3", oversizedName, "hit",
        2, 1450, 5, 0));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_tcp_json_hit_event_format);
    RUN_TEST(test_tcp_json_end_event_includes_missed_targets);
    RUN_TEST(test_tcp_json_ping_event);
    RUN_TEST(test_tcp_serialization_rejects_too_small_buffer);
    RUN_TEST(test_tcp_serialization_rejects_document_overflow);
    return UNITY_END();
}
