#pragma once

#include <ESPAsyncWebServer.h>
#include <atomic>
#include <memory>
#include <mutex>
#include "web_request_queue.h"
#include "round_state.h"

// No AsyncWebServerRequest or AsyncClient pointer crosses into the loop task.
enum class WebRoute
{
  Training, State, Pair, Action, Settings, SaveSettings, ResetWifi,
  WifiSetup, SaveWifi, ScanWifi, Wps, PrepareUpdate
};

struct WebRequest
{
  static std::atomic<unsigned int> liveCount;
  WebRequest() { liveCount.fetch_add(1); }
  ~WebRequest() { liveCount.fetch_sub(1); }
  WebRoute route = WebRoute::State;
  uint32_t queuedAt = 0;
  uint32_t generation = 0;
  unsigned long commandEpoch = 0;
  unsigned long countdownId = 0;
  State expectedState = STANDBY;
  String body;
  String token;
  String name;
  String serverIp;
  String port;
  String delay;
  String ssid;
  String password;
  std::atomic<bool> cancelled{false};
  std::atomic<bool> ready{false};
  std::unique_ptr<AsyncWebServerResponse> response;

  String arg(const char *key) const;
  String header(const char *) const { return token; }
  void send(int code, const char *contentType, const String &content);
};

/// The response is produced by the loop and consumed exclusively by AsyncTCP.
class DeferredWebResponse : public AsyncWebServerResponse
{
public:
  explicit DeferredWebResponse(std::shared_ptr<WebRequest> request);
  ~DeferredWebResponse() override;
  bool _sourceValid() const override { return true; }
  bool _finished() const override;
  bool _failed() const override;
  void _respond(AsyncWebServerRequest *request) override;
  size_t _ack(AsyncWebServerRequest *request, size_t len, uint32_t time) override;

private:
  std::shared_ptr<WebRequest> pending;
  AsyncWebServerResponse *delegate = nullptr;
  std::unique_ptr<AsyncWebServerResponse> timeoutResponse;
  uint32_t createdAt;
};
