
#include "MimeTypes.hpp"

namespace network::http
{
namespace MimeTypes
{

struct Mapping
{
    const char* extension;
    const char* mime_type;
} mappings[] =
{
    { "gif", "image/gif" },
    { "htm", "text/html" },
    { "html", "text/html" },
    { "jpg", "image/jpeg" },
    { "png", "image/png" },
    { "json", "application/json" },
    { "binary", "application/octet-stream" },
    { 0, 0 } // Marks end of list.
};

std::string ExtensionToType(const std::string& extension)
{
    for (Mapping* m = mappings; m->extension; ++m)
    {
        if (m->extension == extension)
        {
            return m->mime_type;
        }
    }

    return "text/plain";
}

} // namespace MimeTypes
} // namespace network::http