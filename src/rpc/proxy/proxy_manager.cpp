#include "proxy_manager.hpp"
#include "fundamental/basic/log.h"
namespace network
{
namespace proxy
{
void ProxyManager::AddWsProxyRoute(const std::string& api_route, ProxyHost host) {
    std::scoped_lock<std::mutex> locker(dataMutex);
    ws_proxy_routes[api_route] = host;
}

void ProxyManager::RemoveWsProxyRoute(const std::string& api_route) {
    std::scoped_lock<std::mutex> locker(dataMutex);
    ws_proxy_routes.erase(api_route);
}

bool ProxyManager::GetWsProxyRoute(const std::string& api_route, ProxyHost& host) {
    std::scoped_lock<std::mutex> locker(dataMutex);
    auto iter = ws_proxy_routes.find(api_route);
    if (iter != ws_proxy_routes.end()) {
        host = iter->second;
        return true;
    }
    return false;
}
} // namespace proxy
} // namespace network