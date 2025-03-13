
#pragma once

#include <string>

namespace network::http
{
namespace MimeTypes {

/// Convert a file extension into a MIME type.
std::string ExtensionToType(const std::string& extension);

} // namespace MimeTypes
} // namespace network::http