#include "mime_types.hpp"

namespace http {
namespace server {
namespace mime_types {

struct mapping
{
  const char* extension;
  const char* mime_type;
} mappings[] =
{
  { "gif", "image/gif" },
  { "htm", "text/html" },
  { "html", "text/html" },
  { "jpg", "image/jpeg" },
  { "png", "image/png" }
};

template <std::size_t N>
std::string find_type(const mapping (&array)[N], const std::string& extension)
{
  for (std::size_t i = 0; i < N; ++i)
  {
    if (array[i].extension == extension)
    {
      return array[i].mime_type;
    }
  }

  return "text/plain";
}

std::string extension_to_type(const std::string& extension)
{
  return find_type(mappings, extension);
}

} // namespace mime_types
} // namespace server
} // namespace http
