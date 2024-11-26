#include "agent_storage.hpp"
#include "fundamental/delay_queue/delay_queue.h"
namespace network
{
namespace proxy
{
void AgentStorage::UpdateAgentInfo(const AgentDataType& id, const AgentDataType& section, AgentDataType&& data)
{
    std::scoped_lock<std::mutex> locker(dataMutex);
    auto& entry     = storage[id][section];
    entry.data      = std::move(data);
    entry.timestamp = Fundamental::Timer::GetTimeNow<std::chrono::seconds, std::chrono::system_clock>();
}
bool AgentStorage::QueryAgentInfo(const AgentDataType& id, const AgentDataType& section, AgentEntryInfo& entry)
{
    std::scoped_lock<std::mutex> locker(dataMutex);
    do
    {
        auto iter = storage.find(id);
        if (iter == storage.end())
            break;
        auto iter2 = iter->second.find(section);
        if (iter2 == iter->second.end())
            break;
        entry.timestamp = iter2->second.timestamp;
        entry.data      = iter2->second.data;
    } while (0);
    return false;
}

void AgentStorage::RemoveExpiredData(std::int64_t expiredSec)
{
    std::scoped_lock<std::mutex> locker(dataMutex);
    auto timeNow      = Fundamental::Timer::GetTimeNow<std::chrono::seconds, std::chrono::system_clock>();
    auto minTimeLimit = timeNow - expiredSec;
    for (auto iter = storage.begin(); iter != storage.end();)
    {
        auto& entrysMap = iter->second;
        for (auto iter2 = entrysMap.begin(); iter2 != entrysMap.end();)
        {
            if (iter2->second.timestamp < minTimeLimit)
            {
                iter2 = entrysMap.erase(iter2);
            }
            else
            {
                iter2++;
            }
        }
        if (entrysMap.empty())
        {
            iter = storage.erase(iter);
        }
        else
        {
            iter++;
        }
    }
}
} // namespace proxy
} // namespace network
