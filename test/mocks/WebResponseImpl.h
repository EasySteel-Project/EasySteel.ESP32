#pragma once
#include "ESPAsyncWebServer.h"

class AsyncBasicResponse : public AsyncWebServerResponse
{
public:
  AsyncBasicResponse(int code, const char *, const char *text) : status(code), body(text) {}
  void _respond(AsyncWebServerRequest *request) override
  {
    request->status = status;
    request->body = body;
    ++request->starts;
  }
  size_t _ack(AsyncWebServerRequest *, size_t length, uint32_t) override
  {
    finished = true;
    return length;
  }
  bool _finished() const override { return finished; }
private:
  int status;
  String body;
  bool finished = false;
};
