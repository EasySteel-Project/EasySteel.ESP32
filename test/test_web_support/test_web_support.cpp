#include <unity.h>
#include "web_body.h"
#include "browser_update.h"
#include "web_template.h"
#include "web_request_queue.h"
#include <memory>

void setUp() {}
void tearDown() {}

void test_fragmented_json_is_complete_and_terminated()
{
  WebBody body;
  TEST_ASSERT_TRUE(body.append(reinterpret_cast<const unsigned char *>("{\"action\":"), 10, 0, 17));
  TEST_ASSERT_TRUE(body.append(reinterpret_cast<const unsigned char *>("\"stop\"}"), 7, 10, 17));
  TEST_ASSERT_EQUAL_STRING("{\"action\":\"stop\"}", body.bytes);
  TEST_ASSERT_EQUAL_UINT32(17, body.size);
}

void test_body_rejects_oversize_out_of_order_and_changed_length()
{
  WebBody body;
  const unsigned char data[] = "abc";
  TEST_ASSERT_FALSE(body.append(data, 3, 0, 513));
  TEST_ASSERT_FALSE(body.append(data, 3, 1, 3));
  TEST_ASSERT_TRUE(body.append(data, 2, 0, 3));
  TEST_ASSERT_FALSE(body.append(data, 1, 2, 4));
  TEST_ASSERT_FALSE(body.append(data, 2, 2, 3));
  TEST_ASSERT_EQUAL_STRING("ab", body.bytes);
}

void test_template_preserves_css_and_streams_owned_replacements()
{
  std::vector<WebTemplateValue> values{{"NAME", "long replacement"}, {"EMPTY", ""}};
  WebTemplate stream("width:100%;%NAME%:%EMPTY%:50%:%UNKNOWN%:%", values);
  values[0].value = "changed";
  std::string actual;
  unsigned char chunk[3];
  size_t count;
  while ((count = stream.read(chunk, sizeof(chunk))) != 0)
  {
    actual.append(reinterpret_cast<char *>(chunk), count);
  }
  TEST_ASSERT_EQUAL_STRING("width:100%;long replacement::50%:%UNKNOWN%:%", actual.c_str());
}

void test_queue_is_bounded_and_preserves_order_across_wrap()
{
  WebRequestQueue<int, 2> queue;
  TEST_ASSERT_TRUE(queue.push(7));
  TEST_ASSERT_TRUE(queue.push(9));
  TEST_ASSERT_FALSE(queue.push(11));
  int value = 0;
  TEST_ASSERT_TRUE(queue.pop(value));
  TEST_ASSERT_EQUAL_INT(7, value);
  TEST_ASSERT_TRUE(queue.push(11));
  TEST_ASSERT_TRUE(queue.pop(value));
  TEST_ASSERT_EQUAL_INT(9, value);
  TEST_ASSERT_TRUE(queue.pop(value));
  TEST_ASSERT_EQUAL_INT(11, value);
  TEST_ASSERT_FALSE(queue.pop(value));
}

void test_queue_releases_consumed_request_and_expiry_wraps()
{
  WebRequestQueue<std::shared_ptr<int>, 2> queue;
  std::shared_ptr<int> item = std::make_shared<int>(1);
  std::weak_ptr<int> observer = item;
  queue.push(item);
  item.reset();
  queue.pop(item);
  item.reset();
  TEST_ASSERT_TRUE(observer.expired());
  TEST_ASSERT_FALSE(webRequestExpired(0x000002e6, 0xffffff00));
  TEST_ASSERT_TRUE(webRequestExpired(0x000002e8, 0xffffff00));
}

void test_update_requires_idle_reservation_and_one_matching_upload()
{
  BrowserUpdateSession update;
  TEST_ASSERT_FALSE(update.reserve(7, 100, false));
  TEST_ASSERT_FALSE(update.claim(7, 100));
  TEST_ASSERT_TRUE(update.reserve(7, 100, true));
  TEST_ASSERT_FALSE(update.reserve(8, 101, true));
  TEST_ASSERT_FALSE(update.claim(8, 101));
  TEST_ASSERT_TRUE(update.claim(7, 101));
  TEST_ASSERT_FALSE(update.claim(7, 102));
  TEST_ASSERT_TRUE(update.acceptChunk(7, 0, 512, 102));
  TEST_ASSERT_FALSE(update.acceptChunk(8, 512, 20, 103));
  TEST_ASSERT_FALSE(update.acceptChunk(7, 0, 20, 103));
  TEST_ASSERT_TRUE(update.acceptChunk(7, 512, 20, 103));
  update.finish(false);
  TEST_ASSERT_FALSE(update.acceptChunk(7, 532, 20, 104));
  update.reset();
  TEST_ASSERT_EQUAL_UINT32(0, update.lease());
  TEST_ASSERT_TRUE(update.reserve(8, 105, true));
}

void test_update_inactivity_and_success_have_distinct_lifetimes()
{
  BrowserUpdateSession update;
  TEST_ASSERT_TRUE(update.reserve(7, 0xfffffff0, true));
  TEST_ASSERT_FALSE(update.expired(0xfffffff0 + uint32_t(14999)));
  TEST_ASSERT_TRUE(update.expired(0xfffffff0 + uint32_t(15000)));
  TEST_ASSERT_FALSE(update.claim(7, 0xfffffff0 + uint32_t(15000)));
  update.reset();
  TEST_ASSERT_TRUE(update.reserve(8, 10, true));
  TEST_ASSERT_TRUE(update.claim(8, 11));
  update.finish(true);
  TEST_ASSERT_FALSE(update.expired(20000));
  TEST_ASSERT_FALSE(update.reserve(9, 20000, true));
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_update_requires_idle_reservation_and_one_matching_upload);
  RUN_TEST(test_update_inactivity_and_success_have_distinct_lifetimes);
  RUN_TEST(test_fragmented_json_is_complete_and_terminated);
  RUN_TEST(test_body_rejects_oversize_out_of_order_and_changed_length);
  RUN_TEST(test_template_preserves_css_and_streams_owned_replacements);
  RUN_TEST(test_queue_is_bounded_and_preserves_order_across_wrap);
  RUN_TEST(test_queue_releases_consumed_request_and_expiry_wraps);
  return UNITY_END();
}
