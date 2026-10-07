#include "web_transport.h"
#include <WebResponseImpl.h>

std::atomic<unsigned int> WebRequest::liveCount{0};

String WebRequest::arg(const char *key) const
{
  if (strcmp(key, "plain") == 0) { return body; }
  if (strcmp(key, "name") == 0) { return name; }
  if (strcmp(key, "serverIp") == 0) { return serverIp; }
  if (strcmp(key, "port") == 0) { return port; }
  if (strcmp(key, "delay") == 0) { return delay; }
  if (strcmp(key, "ssid") == 0) { return ssid; }
  if (strcmp(key, "pass") == 0) { return password; }
  return "";
}

void WebRequest::send(int code, const char *contentType, const String &content)
{
  response.reset(new AsyncBasicResponse(code, contentType, content.c_str()));
  response->addHeader("Cache-Control", "no-store");
}

DeferredWebResponse::DeferredWebResponse(std::shared_ptr<WebRequest> request)
  : pending(std::move(request)), createdAt(millis()) {}

DeferredWebResponse::~DeferredWebResponse()
{
  pending->cancelled.store(true);
}

bool DeferredWebResponse::_finished() const
{
  return delegate != nullptr && delegate->_finished();
}

bool DeferredWebResponse::_failed() const
{
  return delegate != nullptr && delegate->_failed();
}

void DeferredWebResponse::_respond(AsyncWebServerRequest *request)
{
  _ack(request, 0, 0);
}

size_t DeferredWebResponse::_ack(AsyncWebServerRequest *request, size_t len, uint32_t time)
{
  if (delegate == nullptr)
  {
    if (pending->ready.load())
    {
      delegate = pending->response.get();
    }
    else if (static_cast<uint32_t>(millis() - createdAt) >= 2000)
    {
      pending->cancelled.store(true);
      timeoutResponse.reset(new AsyncBasicResponse(503, "application/json", "{\"success\":false}"));
      delegate = timeoutResponse.get();
    }
    if (delegate != nullptr) { delegate->_respond(request); }
    return 0;
  }
  return delegate->_ack(request, len, time);
}
