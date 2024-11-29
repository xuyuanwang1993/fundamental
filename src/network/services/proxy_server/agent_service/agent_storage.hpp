#pragma once
#include "agent_defines.h"
#include <mutex>

namespace network
{
namespace proxy
{

class AgentStorage : public Fundamental::Singleton<AgentStorage>
{
public:
    inline static std::int64_t s_expiredSec = 60;

public:
    AgentStorage();
    ~AgentStorage();
    void UpdateAgentInfo(const AgentDataType& id, const AgentDataType& section, AgentDataType&& data);
    bool QueryAgentInfo(const AgentDataType& id, const AgentDataType& section, AgentEntryInfo& entry);
    void RemoveExpiredData(std::int64_t expiredSec = 60);

private:
    std::mutex dataMutex;
    AgentMap storage;
};

} // namespace proxy
} // namespace network