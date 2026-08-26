#pragma once

#include "tcp_protocol.h"
#include <stddef.h>
#include <stdint.h>

enum class TcpLineFrameResult : uint8_t
{
  None,
  Complete,
  Discarded
};

class TcpLineFramer
{
public:
  TcpLineFramer();

  /// Consume one byte. A completed JSON view remains valid until the next call.
  TcpLineFrameResult push(char value, const char *&json, size_t& length);

  void reset();

private:
  char buffer[TCP_EVENT_MAX_BYTES];
  size_t length;
  bool discarding;
};
