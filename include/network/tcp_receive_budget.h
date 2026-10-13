#pragma once

#include <cstddef>

/// Bound main-loop work even if the peer continuously supplies data.
template <typename Client, typename Consumer>
void tcpReceiveBudget(Client& client, Consumer consume, size_t maxBytes = 512)
{
  for (size_t count = 0; count < maxBytes && client.available() > 0; count++)
  {
    const int value = client.read();
    if (value < 0) { break; }
    consume(static_cast<char>(value));
  }
}
