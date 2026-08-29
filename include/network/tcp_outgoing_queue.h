#pragma once

#include "tcp_protocol.h"
#include <stddef.h>
#include <stdint.h>

enum class TcpMessageKind : uint8_t { Live, ConfirmedResult, Control };

struct TcpOutgoingView
{
  const uint8_t *data;
  size_t length;
};

struct TcpOutgoingCompletion
{
  TcpMessageKind kind;
  char runId[TCP_RUN_ID_BYTES];
};

bool tcpOutgoingQueuePush(
  const char *payload,
  size_t length,
  TcpMessageKind kind,
  const char *runId = nullptr);

bool tcpOutgoingQueuePeek(TcpOutgoingView& view);

/// Consume bytes after a successful non-blocking socket write.
/// Returns true and fills completion when a whole message was sent.
bool tcpOutgoingQueueConsume(size_t bytes, TcpOutgoingCompletion& completion);

void tcpOutgoingQueueClear();
size_t tcpOutgoingQueueGetCount();
