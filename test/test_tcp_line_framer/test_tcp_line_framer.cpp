#include <unity.h>
#include "tcp_line_framer.h"
#include "tcp_line_framer.cpp"

void setUp(void) {}
void tearDown(void) {}

static TcpLineFrameResult pushText(
    TcpLineFramer& framer,
    const char *text,
    const char *&json,
    size_t& length)
{
    TcpLineFrameResult result = TcpLineFrameResult::None;
    for (size_t index = 0; text[index] != '\0'; index++)
    {
        result = framer.push(text[index], json, length);
    }
    return result;
}

void test_fragmented_line_is_completed_once(void)
{
    TcpLineFramer framer;
    const char *json = nullptr;
    size_t length = 0;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TcpLineFrameResult::None),
        static_cast<int>(pushText(framer, "{\"event\":", json, length)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TcpLineFrameResult::Complete),
        static_cast<int>(pushText(framer, "\"ack\"}\n", json, length)));
    TEST_ASSERT_EQUAL_STRING("{\"event\":\"ack\"}", json);
}

void test_crlf_is_accepted(void)
{
    TcpLineFramer framer;
    const char *json = nullptr;
    size_t length = 0;

    TcpLineFrameResult result = pushText(framer, "{}\r\n", json, length);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TcpLineFrameResult::Complete),
        static_cast<int>(result));
    TEST_ASSERT_EQUAL_STRING("{}", json);
}

void test_oversized_line_is_discarded_atomically_and_recovers(void)
{
    TcpLineFramer framer;
    const char *json = nullptr;
    size_t length = 0;
    for (size_t index = 0; index < TCP_JSON_MAX_BYTES + 20; index++)
    {
        framer.push('x', json, length);
    }

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TcpLineFrameResult::Discarded),
        static_cast<int>(framer.push('\n', json, length)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TcpLineFrameResult::Complete),
        static_cast<int>(pushText(framer, "{}\n", json, length)));
    TEST_ASSERT_EQUAL_STRING("{}", json);
}

void test_exact_maximum_payload_is_accepted(void)
{
    TcpLineFramer framer;
    const char *json = nullptr;
    size_t length = 0;
    for (size_t index = 0; index < TCP_JSON_MAX_BYTES; index++)
    {
        framer.push('x', json, length);
    }

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TcpLineFrameResult::Complete),
        static_cast<int>(framer.push('\n', json, length)));
    TEST_ASSERT_EQUAL_UINT32(TCP_JSON_MAX_BYTES, length);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_fragmented_line_is_completed_once);
    RUN_TEST(test_crlf_is_accepted);
    RUN_TEST(test_oversized_line_is_discarded_atomically_and_recovers);
    RUN_TEST(test_exact_maximum_payload_is_accepted);
    return UNITY_END();
}
