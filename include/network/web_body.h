#pragma once

#include <cstddef>
#include <cstring>

struct WebBody
{
  size_t size = 0;
  size_t total = 0;
  char bytes[513] = {};

  bool append(const unsigned char *data, size_t length, size_t index, size_t expected)
  {
    if (expected == 0 || expected >= sizeof(bytes) || index != size ||
        index > expected || length > expected - index || (index != 0 && expected != total))
    {
      return false;
    }
    total = expected;
    memcpy(bytes + index, data, length);
    size += length;
    bytes[size] = '\0';
    return true;
  }
};
