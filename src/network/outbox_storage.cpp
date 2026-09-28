#include "outbox_storage.h"
#include <nvs.h>

static constexpr const char *OUTBOX_NAMESPACE = "es_outbox";
static constexpr const char *OUTBOX_KEY = "snapshot";

OutboxStorageReadResult outboxStorageRead(void *buffer, size_t capacity, size_t& length)
{
  length = 0;
  nvs_handle_t handle = 0;
  const esp_err_t opened = nvs_open(OUTBOX_NAMESPACE, NVS_READONLY, &handle);
  if (opened == ESP_ERR_NVS_NOT_FOUND)
  {
    return OutboxStorageReadResult::Missing;
  }
  if (opened != ESP_OK)
  {
    return OutboxStorageReadResult::Failed;
  }

  length = capacity;
  const esp_err_t result = nvs_get_blob(handle, OUTBOX_KEY, buffer, &length);
  nvs_close(handle);
  if (result == ESP_ERR_NVS_NOT_FOUND)
  {
    return OutboxStorageReadResult::Missing;
  }
  return result == ESP_OK ? OutboxStorageReadResult::Loaded : OutboxStorageReadResult::Failed;
}

bool outboxStorageWrite(const void *buffer, size_t length)
{
  nvs_handle_t handle = 0;
  if (nvs_open(OUTBOX_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK)
  {
    return false;
  }
  const esp_err_t written = nvs_set_blob(handle, OUTBOX_KEY, buffer, length);
  const bool committed = written == ESP_OK && nvs_commit(handle) == ESP_OK;
  nvs_close(handle);
  return committed;
}
