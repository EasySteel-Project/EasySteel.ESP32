#pragma once

#include <ArduinoJson.h>
#include <cstddef>
#include <cstring>

/// Bounded arena for one JSON operation. Reset only after its document is destroyed.
template <size_t Capacity>
class FixedJsonAllocator : public ArduinoJson::Allocator
{
public:
  void reset() { used = 0; }

  void* allocate(size_t size) override
  {
    const size_t alignment = alignof(std::max_align_t);
    const size_t header = sizeof(std::max_align_t);
    const size_t offset = (used + alignment - 1) / alignment * alignment;
    if (offset > Capacity || header > Capacity - offset || size > Capacity - offset - header)
    {
      return nullptr;
    }
    memcpy(buffer + offset, &size, sizeof(size));
    used = offset + header + size;
    return buffer + offset + header;
  }

  void deallocate(void*) override {}

  void* reallocate(void* pointer, size_t size) override
  {
    if (!pointer) { return allocate(size); }
    size_t previous = 0;
    memcpy(&previous, static_cast<unsigned char*>(pointer) - sizeof(std::max_align_t), sizeof(previous));
    if (size <= previous) { return pointer; }
    void* replacement = allocate(size);
    if (replacement) { memcpy(replacement, pointer, previous); }
    return replacement;
  }

private:
  alignas(std::max_align_t) unsigned char buffer[Capacity] = {};
  size_t used = 0;
};
