#include "network/services/proxy_server/proxy_defines.h"
#include "network/services/proxy_server/proxy_request_handler.hpp"
#include "traffic_proxy_defines.h"
namespace network
{
namespace proxy
{

struct TrafficEncoder
{
    static void EncodeProxyRequest(TrafficProxyDataType& dstBuffer, const TrafficProxyRequest& request);
    template <typename Request>
    static void EncodeProxyFrame(ProxyFrame& dstFrame, const Request& request);
};

struct TrafficDecoder
{
    static bool DecodeProxyRequest(const TrafficProxyDataType& srcBuffer, TrafficProxyRequest& dstRequest);
};

template <typename Request>
inline void TrafficEncoder::EncodeProxyFrame(ProxyFrame& dstFrame, const Request& request)
{
    dstFrame.op = ProxyOpCode::TrafficProxyOp;
    EncodeProxyRequest(dstFrame.payload, request);
    ProxyRequestHandler::EncodeFrame(dstFrame);
}

} // namespace proxy
} // namespace network