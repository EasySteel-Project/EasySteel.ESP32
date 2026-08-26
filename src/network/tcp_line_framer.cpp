#include "tcp_line_framer.h"
#include <cstring>

TcpLineFramer::TcpLineFramer()
{
  reset();
}

TcpLineFrameResult TcpLineFramer::push(
  char value,
  const char *&json,
  size_t& jsonLength)
{
  json = nullptr;
  jsonLength = 0;
  if (value == '\n')
  {
    if (discarding)
    {
      reset();
      return TcpLineFrameResult::Discarded;
    }

    if (length == 0)
    {
      return TcpLineFrameResult::None;
    }

    buffer[length] = '\0';
    json = buffer;
    jsonLength = length;
    length = 0;
    return TcpLineFrameResult::Complete;
  }

  if (discarding || value == '\r')
  {
    return TcpLineFrameResult::None;
  }

  if (length >= TCP_JSON_MAX_BYTES)
  {
    length = 0;
    discarding = true;
    return TcpLineFrameResult::None;
  }

  buffer[length++] = value;
  return TcpLineFrameResult::None;
}

void TcpLineFramer::reset()
{
  memset(buffer, 0, sizeof(buffer));
  length = 0;
  discarding = false;
}
