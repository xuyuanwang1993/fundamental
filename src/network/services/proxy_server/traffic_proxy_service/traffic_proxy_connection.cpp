#include "traffic_proxy_connection.hpp"
namespace network
{
namespace proxy
{
TrafficProxyConnection::TrafficProxyConnection(asio::ip::tcp::socket&& socket, ProxyFrame&& frame) :
ProxeServiceBase(std::move(socket), std::move(frame))
{
}

void TrafficProxyConnection::SetUp()
{
}
} // namespace proxy
} // namespace network