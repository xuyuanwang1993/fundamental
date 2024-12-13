#pragma once
#include "proxy_defines.h"

#include <asio.hpp>
#include <functional>
#include <map>
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

class ProxyRequestHandler
{
public:
    using ProtocalHandler = std::function<std::shared_ptr<ProxeServiceBase>(asio::ip::tcp::socket&& socket, ProxyFrame&& frame)>;
    static bool DecodeHeader(const std::uint8_t* data, std::size_t len, ProxyFrame& dstFrame);
    static bool DecodePayload(ProxyFrame& dstFrame);
    static void EncodeFrame(ProxyFrame& dstFrame);
    void UpgradeProtocal(Connection&& connection);
    /// @brief register your own protocal frame
    /// notice:opcode 0 and op code 1 were defined internal
    /// you should use other opcode otherwise an exception will be thrown
    void RegisterProtocal(std::uint8_t opCode, ProtocalHandler handler);
private:
    std::map<std::uint8_t, ProxyRequestHandler::ProtocalHandler> handlers_;
};

} // namespace proxy
} // namespace network