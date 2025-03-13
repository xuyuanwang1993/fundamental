
#pragma once

#include <string>
#include <vector>
#include "Header.hpp"

namespace network::http
{

/// A request received from a client.
struct Request
{
	std::string method;
	std::string uri;
	int http_version_major;
	int http_version_minor;
	std::vector<Header> headers;
    std::string content;
    std::size_t contentTransferred = 0;
    std::size_t contentLength      = (std::size_t)(-1);
    std::string ip;    // remote client's ip,if it's ipv6,than it's ipv6 string
    std::int32_t port; // remote client's port

	bool isDataTransmissionFinishWhenParseAllHeader()
    {
        if (contentLength != (std::size_t)(-1))
            return contentTransferred >= contentLength;
        for (auto& it : headers)
        {
            // It have not decided case-sensitive or not,
            // So just compare "Content-Length" and "content-length" is fine.
            if (it.name == "content-length" || it.name == "Content-Length")
            {
                char* end     = nullptr;
                contentLength = std::strtoull(it.value.c_str(), &end, 10);
                content.reserve(contentLength);
                return contentTransferred >= contentLength;
            }
        }
        contentLength = 0;
        return contentTransferred >= contentLength;
        //return false;
    }
};

} // namespace network::http