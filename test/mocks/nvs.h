#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

using esp_err_t = int;
using nvs_handle_t = uint32_t;
constexpr esp_err_t ESP_OK = 0;
constexpr esp_err_t ESP_FAIL = -1;
constexpr esp_err_t ESP_ERR_NVS_NOT_FOUND = 0x1102;
constexpr esp_err_t ESP_ERR_NVS_INVALID_LENGTH = 0x110c;
enum nvs_open_mode_t { NVS_READONLY, NVS_READWRITE };

struct MockNvsState
{
  std::vector<uint8_t> durable;
  std::vector<uint8_t> staged;
  bool failOpen = false;
  bool failRead = false;
  bool failWrite = false;
  bool failCommit = false;
  bool commitDespiteFailure = false;
  unsigned int writes = 0;
  unsigned int commits = 0;
};

inline MockNvsState& mockNvs()
{
  static MockNvsState state;
  return state;
}

inline void mockNvsReset()
{
  mockNvs() = MockNvsState{};
}

inline esp_err_t nvs_open(const char *name, nvs_open_mode_t mode, nvs_handle_t *handle)
{
  if (mockNvs().failOpen || strcmp(name, "es_outbox") != 0)
  {
    return ESP_FAIL;
  }
  if (mode == NVS_READONLY && mockNvs().durable.empty())
  {
    return ESP_ERR_NVS_NOT_FOUND;
  }
  *handle = 1;
  return ESP_OK;
}

inline esp_err_t nvs_get_blob(nvs_handle_t, const char *key, void *buffer, size_t *length)
{
  if (mockNvs().failRead || strcmp(key, "snapshot") != 0)
  {
    return ESP_FAIL;
  }
  if (mockNvs().durable.empty())
  {
    return ESP_ERR_NVS_NOT_FOUND;
  }
  if (*length < mockNvs().durable.size())
  {
    return ESP_ERR_NVS_INVALID_LENGTH;
  }
  *length = mockNvs().durable.size();
  memcpy(buffer, mockNvs().durable.data(), *length);
  return ESP_OK;
}

inline esp_err_t nvs_set_blob(nvs_handle_t, const char *key, const void *buffer, size_t length)
{
  mockNvs().writes++;
  if (mockNvs().failWrite || strcmp(key, "snapshot") != 0)
  {
    return ESP_FAIL;
  }
  const uint8_t *bytes = static_cast<const uint8_t *>(buffer);
  mockNvs().staged.assign(bytes, bytes + length);
  return ESP_OK;
}

inline esp_err_t nvs_commit(nvs_handle_t)
{
  mockNvs().commits++;
  if (mockNvs().failCommit && !mockNvs().commitDespiteFailure)
  {
    return ESP_FAIL;
  }
  mockNvs().durable = mockNvs().staged;
  return mockNvs().failCommit ? ESP_FAIL : ESP_OK;
}

inline void nvs_close(nvs_handle_t)
{
  mockNvs().staged.clear();
}
