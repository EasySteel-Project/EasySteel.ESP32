#pragma once
#include "Arduino.h"
#include <cstring>

// Response lifecycle contract only; socket I/O requires hardware integration tests.
struct AsyncWebServerRequest
{
  int status = 0;
  String body;
  int starts = 0;
};

class AsyncWebServerResponse
{
public:
  virtual ~AsyncWebServerResponse() = default;
  virtual bool _sourceValid() const { return true; }
  virtual bool _finished() const { return false; }
  virtual bool _failed() const { return false; }
  virtual void _respond(AsyncWebServerRequest *) {}
  virtual size_t _ack(AsyncWebServerRequest *, size_t, uint32_t) { return 0; }
  void addHeader(const char *, const char *) {}
};
