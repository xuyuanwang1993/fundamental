#pragma once
#include "network/services/proxy_server/proxy_request_handler.hpp"
#include "agent_defines.h"
#include <memory>
namespace network
{
namespace proxy
{
// restful
class AgentConnection : public ProxeServiceBase, public std::enable_shared_from_this<AgentConnection>
{
public:
    using CmdHandlerType=std::function<bool(AgentRequestFrame&,AgentResponseFrame&)>;
    inline static std::unordered_map<AgentDataType, CmdHandlerType,Fundamental::BufferHash<AgentSizeType>> cmd_handlers;
public:
    explicit AgentConnection(asio::ip::tcp::socket&& socket, ProxyFrame&& frame);
    void SetUp() override;
    ~AgentConnection();
private:
    void ProcessCmd();
    bool PaserRequestFrame(AgentRequestFrame &request);
    void HandleResponse(AgentResponseFrame &response);
};
} // namespace proxy
} // namespace network