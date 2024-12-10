#pragma once
#include "proxy_defines.h"

#include <asio.hpp>
#include <functional>
#include <memory>

#include "fundamental/basic/utils.hpp"
namespace network
{
namespace proxy
{
class Connection;
struct ProxeServiceBase : public Fundamental::NonCopyable
{
    explicit ProxeServiceBase(asio::ip::tcp::socket&& socket, ProxyFrame&& frame);
    virtual ~ProxeServiceBase();
    // this function will be called in ProxyRequestHandler::UpgradeProtocal
    virtual void SetUp() = 0;
    /// Socket for the connection.
    asio::ip::tcp::socket socket_;
    ProxyFrame frame;
};

struct ProxyRequestHandler
{
    using ProtocalHandler = std::function<std::shared_ptr<ProxeServiceBase>(asio::ip::tcp::socket&& socket, ProxyFrame&& frame)>;
    static bool DecodeHeader(const std::uint8_t* data, std::size_t len, ProxyFrame& dstFrame);
    static bool DecodePayload(ProxyFrame& dstFrame);
    static void EncodeFrame(ProxyFrame& dstFrame);
    static void UpgradeProtocal(Connection&& connection);
    /// @brief register your own protocal frame
    /// notice:opcode 0 and op code 1 were defined internal
    /// you should use other opcode otherwise an exception will be thrown
    static void RegisterProtocal(std::uint8_t opCode, ProtocalHandler handler);
};

} // namespace proxy
} // namespace network