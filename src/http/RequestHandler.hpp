
#pragma once

#include <string>
#include "fundamental/basic/utils.hpp"

namespace network::http
{

struct Reply;
struct Request;

/// The common handler for all incoming requests.
class RequestHandler
  : private Fundamental::NonCopyable
{
public:
    /// Construct with a directory containing files to be served.
    explicit RequestHandler(const std::string& docRoot);

    /// Handle a request and produce a reply.
    void HandleRequest(const Request& req, Reply& rep);

private:
    /// The directory containing the files to be served.
    std::string m_docRoot;

    /// Perform URL-decoding on a string. Returns false if the encoding was
    /// invalid.
    static bool UrlDecode(const std::string& in, std::string& out);
};

} // namespace network::http