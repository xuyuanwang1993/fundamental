#pragma once
#include "agent_defines.h"
#include "fundamental/basic/utils.hpp"
#include "network/services/proxy_server/proxy_request_handler.hpp"
#include <asio.hpp>
#include <functional>
#include <map>
#include <mutex>
namespace network
{
namespace proxy
{
namespace rpc
{
struct AgentResponse
{
    std::int32_t code;
    std::string msg;
    std::vector<std::uint8_t> data;
};
using AgentClientToken      = std::intptr_t;
using AgentFinishedCallback = std::function<void(bool, AgentClientToken, AgentResponse&&)>;
struct AgentUpdateContext
{
    std::string host;
    std::string service;
    AgentUpdateRequest request;
    AgentFinishedCallback cb;
};

struct AgentQueryContext
{
    std::string host;
    std::string service;
    AgentQueryRequest request;
    AgentFinishedCallback cb;
};

class AgentClient
{
public:
    struct AgentSession;
    struct AgentHandler;
    friend struct AgentHandler;
public:
    AgentClient();
    ~AgentClient();
    // this function just post cancel action
    bool CancelRequest(AgentClientToken token);
    AgentClientToken Update(const AgentUpdateContext& context);
    AgentClientToken Query(const AgentQueryContext& context);
protected:
    std::mutex mutex_;
    std::map<AgentClientToken, std::weak_ptr<AgentSession>> sessions_;
};
} // namespace rpc
} // namespace proxy
} // namespace network