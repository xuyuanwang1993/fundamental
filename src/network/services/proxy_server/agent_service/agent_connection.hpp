#pragma once
#include "network/services/proxy_server/proxy_request_handler.hpp"
#include <memory>
namespace network
{
namespace proxy
{
class AgentConnection : public ProxeServiceBase, public std::enable_shared_from_this<AgentConnection>
{
public:
    explicit AgentConnection(asio::ip::tcp::socket&& socket, ProxyFrame&& frame);
    void SetUp() override;
};
} // namespace proxy
} // namespace network