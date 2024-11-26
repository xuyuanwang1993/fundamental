#pragma once
#include "proxy_defines.h"
namespace network
{
namespace proxy
{
class Connection;
struct ProxyRequestHandler
{
    static bool DecodeHeader(const std::uint8_t *data,std::size_t len,ProxyFrame &dstFrame);
    static bool DecodePayload(ProxyFrame &dstFrame);
    static void EncodeFrame(ProxyFrame &dstFrame);
    static void UpgradeProtocal(Connection &&connection);
};

}
}