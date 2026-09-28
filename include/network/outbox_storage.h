#pragma once

#include <stddef.h>

enum class OutboxStorageReadResult { Missing, Loaded, Failed };

/// Read the single NVS snapshot without erasing or repairing invalid storage.
OutboxStorageReadResult outboxStorageRead(void *buffer, size_t capacity, size_t& length);

/// Replace and commit the single NVS blob. Call only outside active rounds.
bool outboxStorageWrite(const void *buffer, size_t length);
