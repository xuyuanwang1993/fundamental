#pragma once
#include "network/services/proxy_server/proxy_request_handler.hpp"
#include <memory>
namespace network
{
namespace proxy
{
class TrafficProxyConnection : public ProxeServiceBase, public std::enable_shared_from_this<TrafficProxyConnection>
{
public:
    explicit TrafficProxyConnection(asio::ip::tcp::socket&& socket, ProxyFrame&& frame);
    void SetUp() override;
};
}
}