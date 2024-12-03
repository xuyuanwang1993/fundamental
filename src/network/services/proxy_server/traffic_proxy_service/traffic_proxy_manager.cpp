#include "traffic_proxy_manager.hpp"
#include "fundamental/basic/log.h"
namespace network
{
namespace proxy
{
void TrafficProxyManager::UpdateTrafficProxyHostInfo(const TrafficProxyDataType& serviceName, TrafficProxyHostInfo&& hostInfo)
{
    std::scoped_lock<std::mutex> locker(dataMutex);
    storage[serviceName] = std::move(hostInfo);
}

void TrafficProxyManager::RemoveTrafficProxyHostInfo(const TrafficProxyDataType& serviceName)
{
    std::scoped_lock<std::mutex> locker(dataMutex);
    storage.erase(serviceName);
}

bool TrafficProxyManager::GetTrafficProxyHostInfo(const TrafficProxyDataType& serviceName,
                                                  const TrafficProxyDataType& token,
                                                  const TrafficProxyDataType& field,
                                                  TrafficProxyHost& outHost)
{
    std::scoped_lock<std::mutex> locker(dataMutex);
    do
    {
        auto iter = storage.find(serviceName);
        if (iter == storage.end())
        {
            FERR("service:{} is not existed", serviceName.ToString());
            break;
        }
        if (iter->second.token && iter->second.token != token)
        {
            FERR("service:{} token:{} is invalid", serviceName.ToString(), token.ToString());
            break;
        }
        auto iter2 = iter->second.hosts.find(field);
        if (iter2 == iter->second.hosts.end())
            iter2 = iter->second.hosts.find(std::string(kDefaultFieldName));
        if (iter2 == iter->second.hosts.end())
        {
            FERR("service:{} is not available for field:{}", serviceName.ToString(), field.ToString());
            break;
        }
        outHost = iter2->second;
        return true;
    } while (0);
    return false;
}
} // namespace proxy
} // namespace network