#include <unity.h>
#include "tcp_line_framer.h"
#include "tcp_line_framer.cpp"
#include "../support/protocol_fixture_loader.h"
#include <string>

void setUp(void)
{
}

void tearDown(void)
{
}

void test_shared_framing_vectors(void)
{
    JsonDocument manifest = loadProtocolFixtureManifest();
    size_t executedVectors = 0;

    for (JsonObjectConst entry : manifest.as<JsonArrayConst>())
    {
        if (!protocolFixtureLayerMatches(entry, "framing"))
        {
            continue;
        }

        executedVectors++;
        const char *fixtureId = protocolFixtureId(entry);
        const std::string bytes =
            loadProtocolFixture(entry["file"] | "");
        const size_t chunkSize = entry["chunkSize"] | bytes.size();
        TcpLineFramer framer;
        const char *json = nullptr;
        size_t jsonLength = 0;
        size_t completeCount = 0;
        size_t discardedCount = 0;
        std::string lastPayload;

        for (size_t offset = 0; offset < bytes.size(); offset += chunkSize)
        {
            const size_t length =
                std::min(chunkSize, bytes.size() - offset);
            TcpLineFrameResult chunkResult = TcpLineFrameResult::None;

            for (size_t index = 0; index < length; index++)
            {
                chunkResult = framer.push(
                    bytes[offset + index],
                    json,
                    jsonLength);
                if (chunkResult == TcpLineFrameResult::Complete)
                {
                    completeCount++;
                    lastPayload.assign(json, jsonLength);
                }
                else if (chunkResult == TcpLineFrameResult::Discarded)
                {
                    discardedCount++;
                }
            }

            if (entry["chunkSize"].is<size_t>() &&
                offset + length < bytes.size())
            {
                TEST_ASSERT_EQUAL_INT_MESSAGE(
                    static_cast<int>(TcpLineFrameResult::None),
                    static_cast<int>(chunkResult),
                    fixtureId);
            }
        }

        const size_t expectedCompleteCount =
            entry["expectedCompleteCount"] | 0;
        const size_t expectedDiscardedCount =
            entry["expectedDiscardedCount"] | 0;
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            expectedCompleteCount,
            completeCount,
            fixtureId);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            expectedDiscardedCount,
            discardedCount,
            fixtureId);

        if (entry["expectedPayload"].is<const char *>())
        {
            TEST_ASSERT_EQUAL_STRING_MESSAGE(
                entry["expectedPayload"].as<const char *>(),
                lastPayload.c_str(),
                fixtureId);
        }

        if (entry["expectedPayloadBytes"].is<size_t>())
        {
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(
                entry["expectedPayloadBytes"].as<size_t>(),
                lastPayload.size(),
                fixtureId);
        }
    }

    TEST_ASSERT_GREATER_THAN_MESSAGE(
        0,
        executedVectors,
        "No shared framing vectors were executed.");
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_shared_framing_vectors);
    return UNITY_END();
}
