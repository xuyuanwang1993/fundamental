#pragma once
#include "fundamental/basic/buffer.hpp"
#include <unordered_map>
namespace network
{
namespace proxy
{
using TrafficProxySizeType = std::uint32_t;
using TrafficProxyDataType = Fundamental::Buffer<TrafficProxySizeType>;
enum TrafficProxyOperation : std::int32_t
{
    TrafficProxyDataOp = 0,
};

struct TrafficProxyRequestBase
{
    TrafficProxyOperation op = TrafficProxyDataOp;
};

struct TrafficProxyRequest : TrafficProxyRequestBase
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