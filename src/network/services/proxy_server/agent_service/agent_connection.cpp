#include "agent_connection.hpp"

namespace network
{
namespace proxy
{
AgentConnection::AgentConnection(asio::ip::tcp::socket&& socket, ProxyFrame&& frame) :
ProxeServiceBase(std::move(socket), std::move(frame))
{
}
void AgentConnection::SetUp()
{
}
} // namespace proxy
} // namespace network
