#include <unity.h>
#include "tcp_receive_budget.h"
#include "tcp_line_framer.cpp"
#include "fixed_json_allocator.h"
#include <string>
#include <limits>

void setUp() {}
void tearDown() {}

struct Source
{
  std::string text;
  size_t position = 0;
  bool continuous = false;
  int available() { return continuous || position < text.size(); }
  int read() { return continuous ? 'x' : text[position++]; }
};

void test_continuous_sender_returns_after_budget_and_fragmented_frames_survive()
{
  Source flood; flood.continuous = true;
  size_t count = 0;
  tcpReceiveBudget(flood, [&](char) { count++; });
  TEST_ASSERT_EQUAL(512, count);
  Source finite; finite.text = "{\"event\":\"ping\"}\n";
  TcpLineFramer framer;
  std::string completed;
  auto consume = [&](char value) {
    const char* json = nullptr; size_t length = 0;
    if (framer.push(value, json, length) == TcpLineFrameResult::Complete) { completed.assign(json, length); }
  };
  tcpReceiveBudget(finite, consume, 5);
  TEST_ASSERT_TRUE(completed.empty());
  tcpReceiveBudget(finite, consume);
  TEST_ASSERT_EQUAL_STRING("{\"event\":\"ping\"}", completed.c_str());
}

void test_arena_bounds_growth_alignment_and_exhaustion()
{
  FixedJsonAllocator<512> arena;
  void* memory = arena.allocate(4);
  TEST_ASSERT_NOT_NULL(memory);
  TEST_ASSERT_EQUAL(0, reinterpret_cast<uintptr_t>(memory) % alignof(std::max_align_t));
  memcpy(memory, "abc", 4);
  void* grown = arena.reallocate(memory, 16);
  TEST_ASSERT_NOT_NULL(grown);
  TEST_ASSERT_EQUAL_STRING("abc", static_cast<char*>(grown));
  TEST_ASSERT_NULL(arena.allocate(std::numeric_limits<size_t>::max()));
  arena.reset();
  JsonDocument document(&arena);
  deserializeJson(document, "{\"value\":1}");
  TEST_ASSERT_TRUE(document.overflowed());
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_continuous_sender_returns_after_budget_and_fragmented_frames_survive);
  RUN_TEST(test_arena_bounds_growth_alignment_and_exhaustion);
  return UNITY_END();
}
