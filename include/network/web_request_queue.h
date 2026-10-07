#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

/// Fixed-capacity FIFO. The caller supplies synchronization between tasks.
template <typename T, size_t Capacity>
class WebRequestQueue
{
public:
  bool push(T value)
  {
    if (count == Capacity) { return false; }
    entries[(head + count) % Capacity] = std::move(value);
    ++count;
    return true;
  }

  bool pop(T &value)
  {
    if (count == 0) { return false; }
    value = std::move(entries[head]);
    entries[head] = T{};
    head = (head + 1) % Capacity;
    --count;
    return true;
  }

private:
  std::array<T, Capacity> entries{};
  size_t head = 0;
  size_t count = 0;
};

inline bool webRequestExpired(uint32_t now, uint32_t queuedAt)
{
  return static_cast<uint32_t>(now - queuedAt) >= 1000;
}
