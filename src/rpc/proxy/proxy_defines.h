#pragma once

#include "fundamental/basic/buffer.hpp"

#include <functional>
#include <unordered_map>

namespace network
{
namespace proxy
{
using proxy_update_func = std::function<void(std::string& /*service*/, std::string& /*service*/)>;
struct ProxyHost {
    std::string host;
    std::string service;
    proxy_update_func update_func;
    void update() {
        if (!update_func) return;
        update_func(host, service);
    }
};
using ProxyHostFieldMap = std::unordered_map<std::string /*field*/, ProxyHost>;
struct ProxyHostInfo {
    std::string token;
    ProxyHostFieldMap hosts;
};

using ProxyHostMap = std::unordered_map<std::string /*proxyServiceName*/, ProxyHostInfo>;

} // namespace proxy
} // namespace network