#pragma once
#include "proxy_defines.h"
#include "fundamental/basic/utils.hpp"
#include <asio.hpp>
namespace network
{
namespace proxy
{
class Connection;
struct ProxyRequestHandler
{
    static bool DecodeHeader(const std::uint8_t* data, std::size_t len, ProxyFrame& dstFrame);
    static bool DecodePayload(ProxyFrame& dstFrame);
    static void EncodeFrame(ProxyFrame& dstFrame);
    static void UpgradeProtocal(Connection&& connection);
    static std::vector<asio::const_buffer> FrameToBuffers(const ProxyFrame& frame);
};

struct ProxeServiceBase:public Fundamental::NonCopyable
{
    explicit ProxeServiceBase(asio::ip::tcp::socket&& socket, ProxyFrame&& frame);
    virtual ~ProxeServiceBase();
    // this function will be called in ProxyRequestHandler::UpgradeProtocal
    virtual void SetUp() = 0;
    /// Socket for the connection.
    asio::ip::tcp::socket socket_;
    ProxyFrame frame;
};
} // namespace proxy
} // namespace network