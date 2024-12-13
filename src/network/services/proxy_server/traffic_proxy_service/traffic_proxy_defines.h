#pragma once

#include "network/services/proxy_server/proxy_defines.h"
#include "fundamental/basic/buffer.hpp"

#include <unordered_map>

namespace network
{
namespace proxy
{
using TrafficProxySizeType = ProxySizeType;
using TrafficProxyDataType = Fundamental::Buffer<TrafficProxySizeType>;
enum TrafficProxyOperation : std::int32_t
{
    TrafficProxyDataOp           = 0,
    UpdateTrafficProxyHostInfoOp = 1,
    RemoveTrafficProxyHostInfoOp = 2
};

struct TrafficProxyRequestBase
{
    const TrafficProxyOperation op = TrafficProxyDataOp;
};

struct TrafficProxyResponseBase
{
    const TrafficProxyOperation op = TrafficProxyDataOp;
    std::int32_t req;
};

struct TrafficProxyRequest : TrafficProxyRequestBase
{
    TrafficProxyRequest() :
    TrafficProxyRequestBase { TrafficProxyDataOp }
    {
    }
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