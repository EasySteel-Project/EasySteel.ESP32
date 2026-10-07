#include <unity.h>
#include "web_transport.cpp"
#include "mock_globals.cpp"

void setUp() { g_mockMillis = 100; }
void tearDown() { TEST_ASSERT_EQUAL_UINT32(0, WebRequest::liveCount.load()); }

void test_response_publishes_status_and_body_only_after_loop_completion()
{
  std::shared_ptr<WebRequest> job = std::make_shared<WebRequest>();
  DeferredWebResponse response(job);
  AsyncWebServerRequest socket;
  response._respond(&socket);
  TEST_ASSERT_EQUAL_INT(0, socket.status);
  TEST_ASSERT_FALSE(response._finished());
  job->send(409, "application/json", "{\"error\":\"busy\"}");
  job->ready.store(true);
  response._ack(&socket, 0, 0);
  TEST_ASSERT_EQUAL_INT(409, socket.status);
  TEST_ASSERT_EQUAL_STRING("{\"error\":\"busy\"}", socket.body.c_str());
  response._ack(&socket, socket.body.length(), 0);
  TEST_ASSERT_TRUE(response._finished());
  TEST_ASSERT_EQUAL_INT(1, socket.starts);
}

void test_disconnect_cancels_request_without_dangling_loop_data()
{
  std::shared_ptr<WebRequest> job = std::make_shared<WebRequest>();
  job->body = "owned request data";
  {
    DeferredWebResponse response(job);
    TEST_ASSERT_FALSE(job->cancelled.load());
  }
  TEST_ASSERT_TRUE(job->cancelled.load());
  TEST_ASSERT_EQUAL_STRING("owned request data", job->body.c_str());
  // A response completed concurrently with disconnect is still owned until job release.
  job->send(202, "application/json", "{}");
  job->ready.store(true);
}

void test_unprocessed_request_times_out_once_and_ignores_late_response()
{
  std::shared_ptr<WebRequest> job = std::make_shared<WebRequest>();
  DeferredWebResponse response(job);
  AsyncWebServerRequest socket;
  response._respond(&socket);
  g_mockMillis = 2100;
  response._ack(&socket, 0, 0);
  TEST_ASSERT_EQUAL_INT(503, socket.status);
  TEST_ASSERT_TRUE(job->cancelled.load());
  job->send(200, "text/plain", "too late");
  job->ready.store(true);
  response._ack(&socket, 1, 0);
  TEST_ASSERT_EQUAL_INT(503, socket.status);
  TEST_ASSERT_EQUAL_INT(1, socket.starts);
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_response_publishes_status_and_body_only_after_loop_completion);
  RUN_TEST(test_disconnect_cancels_request_without_dangling_loop_data);
  RUN_TEST(test_unprocessed_request_times_out_once_and_ignores_late_response);
  return UNITY_END();
}
