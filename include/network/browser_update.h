#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

enum class BrowserUpdateState { Idle, Prepared, Uploading, Success, Failed };

/// Mutating operations require the caller's update mutex; observations are atomic.
class BrowserUpdateSession
{
public:
  BrowserUpdateState state() const { return current.load(); }
  uint32_t lease() const { return owner.load(); }

  bool reserve(uint32_t lease, uint32_t now, bool idle)
  {
    if (!idle || lease == 0 || state() != BrowserUpdateState::Idle) { return false; }
    owner.store(lease);
    activity.store(now);
    bytes = 0;
    current.store(BrowserUpdateState::Prepared);
    return true;
  }

  bool claim(uint32_t lease, uint32_t now)
  {
    if (lease == 0 || lease != owner.load() || state() != BrowserUpdateState::Prepared || expired(now))
    {
      return false;
    }
    activity.store(now);
    current.store(BrowserUpdateState::Uploading);
    return true;
  }

  bool acceptChunk(uint32_t lease, size_t index, size_t length, uint32_t now)
  {
    if (state() != BrowserUpdateState::Uploading || lease != owner.load() ||
        index != bytes || length > SIZE_MAX - bytes) { return false; }
    bytes += length;
    activity.store(now);
    return true;
  }

  bool expired(uint32_t now) const
  {
    return (state() == BrowserUpdateState::Prepared || state() == BrowserUpdateState::Uploading) &&
      static_cast<uint32_t>(now - activity.load()) >= 15000;
  }

  void finish(bool success)
  {
    current.store(success ? BrowserUpdateState::Success : BrowserUpdateState::Failed);
  }

  void reset()
  {
    owner.store(0);
    current.store(BrowserUpdateState::Idle);
  }

private:
  std::atomic<BrowserUpdateState> current{BrowserUpdateState::Idle};
  std::atomic<uint32_t> owner{0};
  std::atomic<uint32_t> activity{0};
  size_t bytes = 0;
};
