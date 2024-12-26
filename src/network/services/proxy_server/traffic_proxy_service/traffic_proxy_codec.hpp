#pragma once
#include "network/services/proxy_server/proxy_defines.h"
#include "network/services/proxy_server/proxy_request_handler.hpp"
#include "traffic_proxy_defines.h"
namespace network {
namespace proxy {

struct TrafficEncoder {
    template <typename CommandFrame>
    static void EncodeCommandFrame(TrafficProxyDataType& dstBuffer, const CommandFrame& command_frame);
    template <typename CommandFrame>
    static void EncodeProxyFrame(ProxyFrame& dstFrame, const CommandFrame& command_frame);
};

struct TrafficDecoder {
    template <typename CommandFrame>
    static bool DecodeCommandFrame(const TrafficProxyDataType& srcBuffer, CommandFrame& dst_command_frame);
};

template <typename CommandFrame>
inline void TrafficEncoder::EncodeProxyFrame(ProxyFrame& dstFrame, const CommandFrame& command_frame) {
    dstFrame.op = kTrafficProxyOpcode;
    EncodeCommandFrame(dstFrame.payload, command_frame);
    ProxyRequestHandler::EncodeFrame(dstFrame);
}
} // namespace proxy
} // namespace network