#pragma once

#include "fundamental/basic/buffer.hpp"

#include <unordered_map>

namespace network {
namespace proxy {

struct ProxyHost {
    std::string host;
    std::string service;
};
using ProxyHostFieldMap =
    std::unordered_map<std::string /*field*/, ProxyHost>;
struct ProxyHostInfo {
    std::string token;
    ProxyHostFieldMap hosts;
};

using ProxyHostMap = std::unordered_map<std::string /*proxyServiceName*/, ProxyHostInfo>;

} // namespace proxy
} // namespace network