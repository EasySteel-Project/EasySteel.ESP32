#include "tcp_outgoing_queue.h"
#include <cstdio>
#include <cstring>

struct QueuedMessage
{
  char payload[TCP_EVENT_MAX_BYTES];
  size_t length;
  size_t offset;
  TcpMessageKind kind;
  char runId[24];
};

static constexpr size_t OUTGOING_QUEUE_CAPACITY = 16;
static QueuedMessage outgoingQueue[OUTGOING_QUEUE_CAPACITY];
static size_t outgoingHead = 0;
static size_t outgoingTail = 0;
static size_t outgoingCount = 0;

bool tcpOutgoingQueuePush(
  const char *payload,
  size_t length,
  TcpMessageKind kind,
  const char *runId)
{
  if (!payload || length == 0 || length > TCP_EVENT_MAX_BYTES ||
      outgoingCount >= OUTGOING_QUEUE_CAPACITY)
    return false;

  QueuedMessage& message = outgoingQueue[outgoingHead];
  memcpy(message.payload, payload, length);
  message.length = length;
  message.offset = 0;
  message.kind = kind;
  snprintf(message.runId, sizeof(message.runId), "%s", runId ? runId : "");

  outgoingHead = (outgoingHead + 1) % OUTGOING_QUEUE_CAPACITY;
  outgoingCount++;
  return true;
}

bool tcpOutgoingQueuePeek(TcpOutgoingView& view)
{
  if (outgoingCount == 0)
    return false;

  const QueuedMessage& message = outgoingQueue[outgoingTail];
  view.data = reinterpret_cast<const uint8_t *>(message.payload + message.offset);
  view.length = message.length - message.offset;
  return true;
}

bool tcpOutgoingQueueConsume(size_t bytes, TcpOutgoingCompletion& completion)
{
  if (outgoingCount == 0 || bytes == 0)
    return false;

  QueuedMessage& message = outgoingQueue[outgoingTail];
  const size_t remaining = message.length - message.offset;
  message.offset += bytes > remaining ? remaining : bytes;
  if (message.offset < message.length)
    return false;

  completion.kind = message.kind;
  snprintf(completion.runId, sizeof(completion.runId), "%s", message.runId);
  outgoingTail = (outgoingTail + 1) % OUTGOING_QUEUE_CAPACITY;
  outgoingCount--;
  return true;
}

void tcpOutgoingQueueClear()
{
  outgoingHead = 0;
  outgoingTail = 0;
  outgoingCount = 0;
}

size_t tcpOutgoingQueueGetCount()
{
  return outgoingCount;
}
