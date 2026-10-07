#pragma once

#include <cstring>
#include <string>
#include <vector>
#include <utility>

struct WebTemplateValue
{
  const char *key;
  std::string value;
};

/// Own replacement values while streaming the immutable flash template in bounded pieces.
class WebTemplate
{
public:
  WebTemplate(const char *source, std::vector<WebTemplateValue> values)
    : source(source), values(std::move(values)) {}

  size_t read(unsigned char *out, size_t capacity)
  {
    size_t written = 0;
    while (written < capacity)
    {
      if (replacement != nullptr)
      {
        if (replacementOffset < replacement->size())
        {
          out[written++] = (*replacement)[replacementOffset++];
          continue;
        }
        replacement = nullptr;
      }
      if (*source == '\0') { break; }
      if (*source == '%')
      {
        bool matched = false;
        for (const WebTemplateValue &entry : values)
        {
          size_t length = std::strlen(entry.key);
          if (std::strncmp(source + 1, entry.key, length) == 0 && source[length + 1] == '%')
          {
            source += length + 2;
            replacement = &entry.value;
            replacementOffset = 0;
            matched = true;
            break;
          }
        }
        if (matched) { continue; }
      }
      out[written++] = *source++;
    }
    return written;
  }

private:
  const char *source;
  std::vector<WebTemplateValue> values;
  const std::string *replacement = nullptr;
  size_t replacementOffset = 0;
};
