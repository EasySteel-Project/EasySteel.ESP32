#pragma once

#include <cstdint>

// Socket operations must return immediately. Ownership transfers only on success.
template <typename Socket>
class TcpConnectAttempt
{
public:
  bool pending() const { return fd_ >= 0; }

  void begin(uint32_t address, uint16_t port, uint32_t now)
  {
    cancel();
    startedAt_ = now;
    fd_ = Socket::open(address, port);
  }

  int poll(uint32_t now, bool allowed)
  {
    if (!pending()) { return -1; }
    if (!allowed || static_cast<uint32_t>(now - startedAt_) >= 150)
    {
      cancel();
      return -1;
    }
    const int result = Socket::poll(fd_);
    if (result < 0) { cancel(); }
    if (result <= 0) { return -1; }
    const int connected = fd_;
    fd_ = -1;
    return connected;
  }

  void cancel()
  {
    if (pending()) { Socket::close(fd_); }
    fd_ = -1;
  }

private:
  int fd_ = -1;
  uint32_t startedAt_ = 0;
};
