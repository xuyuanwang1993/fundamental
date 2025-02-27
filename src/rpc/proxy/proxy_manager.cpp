#include "proxy_manager.hpp"
#include "fundamental/basic/log.h"
namespace network {
namespace proxy {
void ProxyManager::UpdateProxyHostInfo(const std::string& serviceName, ProxyHostInfo&& hostInfo) {
    std::scoped_lock<std::mutex> locker(dataMutex);
    storage[serviceName] = std::move(hostInfo);
}

void ProxyManager::RemoveProxyHostInfo(const std::string& serviceName) {
    std::scoped_lock<std::mutex> locker(dataMutex);
    storage.erase(serviceName);
}

bool ProxyManager::GetProxyHostInfo(const std::string& serviceName, const std::string& token, const std::string& field,
                                    ProxyHost& outHost) {
    std::scoped_lock<std::mutex> locker(dataMutex);
    do {
        auto iter = storage.find(serviceName);
        if (iter == storage.end()) {
            FDEBUG("service:{} is not existed", serviceName);
            break;
        }
        if (iter->second.token != token) {
            FDEBUG("service:{} token:{} is invalid", serviceName, token);
            break;
        }
        auto iter2 = iter->second.hosts.find(field);
        if (iter2 == iter->second.hosts.end()) iter2 = iter->second.hosts.find(std::string(kDefaultFieldName));
        if (iter2 == iter->second.hosts.end()) {
            FDEBUG("service:{} is not available for field:{}", serviceName, field);
            break;
        }
        outHost = iter2->second;
        return true;
    } while (0);
    return false;
}
} // namespace proxy
} // namespace network