#include "agent_connection.hpp"
#include  "fundamental/basic/buffer.hpp"
namespace network
{
namespace proxy
{
namespace details
{
struct AgentRequestPayload
{
    
}; 
}

AgentConnection::AgentConnection(asio::ip::tcp::socket&& socket, ProxyFrame&& frame) :
ProxeServiceBase(std::move(socket), std::move(frame))
{
}

void AgentConnection::SetUp()
{
    ProcessCmd();
}
void AgentConnection::ProcessCmd()
{

}
} // namespace proxy
} // namespace network
