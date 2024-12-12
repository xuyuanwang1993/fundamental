#pragma once
#include "agent_defines.h"
#include "network/services/proxy_server/proxy_request_handler.hpp"
#include <memory>
namespace network
{
namespace proxy
{
// restful
class AgentConnection : public ProxeServiceBase, public std::enable_shared_from_this<AgentConnection>
{
public:
    struct details;
    friend struct details;

public:
    using CmdHandlerType = std::function<void(AgentRequestFrame&, std::shared_ptr<AgentConnection>)>;
    inline static std::unordered_map<AgentDataType, CmdHandlerType, Fundamental::BufferHash<AgentSizeType>> cmd_handlers;

public:
    void SetUp() override;
    ~AgentConnection();
    static std::shared_ptr<AgentConnection> MakeShared(asio::ip::tcp::socket&& socket, ProxyFrame&& frame)
    {
        return std::shared_ptr<AgentConnection>(new AgentConnection(std::move(socket), std::move(frame)));
    }

protected:
    explicit AgentConnection(asio::ip::tcp::socket&& socket, ProxyFrame&& frame);

private:
    void ProcessCmd();
    bool PaserRequestFrame(AgentRequestFrame& request);
    void HandleResponse(AgentResponseFrame& response);
    void HandleDelayRequest(const std::function<bool()>& check_func);
    void TimeoutProcess(const std::function<bool()>& check_func);

private:
    std::shared_ptr<asio::steady_timer> delay_timer;
    std::uint8_t delay_buf;
};
} // namespace proxy
} // namespace network