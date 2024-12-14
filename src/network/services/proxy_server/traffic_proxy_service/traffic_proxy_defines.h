#pragma once

#include "network/services/proxy_server/proxy_defines.h"
#include "fundamental/basic/buffer.hpp"

#include <unordered_map>

namespace network
{
namespace proxy
{
constexpr std::uint8_t kTrafficProxyOpcode = 1;
using TrafficProxySizeType = ProxySizeType;
using TrafficProxyDataType = Fundamental::Buffer<TrafficProxySizeType>;

struct TrafficProxyRequest
{
    TrafficProxyDataType proxyServiceName;
    TrafficProxyDataType field;
    TrafficProxyDataType token;
};

struct TrafficProxyHost
{
    TrafficProxyDataType host;
    TrafficProxyDataType service;
};
using TrafficProxyHostFieldMap = std::unordered_map<TrafficProxyDataType /*field*/,
                                                    TrafficProxyHost,
                                                    Fundamental::BufferHash<TrafficProxySizeType>>;
struct TrafficProxyHostInfo
{
    TrafficProxyDataType token;
    TrafficProxyHostFieldMap hosts;
};

using TrafficProxyHostMap = std::unordered_map<TrafficProxyDataType /*proxyServiceName*/,
                                               TrafficProxyHostInfo,
                                               Fundamental::BufferHash<TrafficProxySizeType>>;


} // namespace proxy
} // namespace network