#pragma once
#include "proxy_defines.h"
#include <mutex>

#include "fundamental/basic/utils.hpp"

namespace network
{
namespace proxy
{

class ProxyManager {
public:
    virtual ~ProxyManager() = default;
    virtual void AddWsProxyRoute(const std::string& api_route, ProxyHost host);
    virtual void RemoveWsProxyRoute(const std::string& api_route);
    virtual bool GetWsProxyRoute(const std::string& api_route, ProxyHost& host);

private:
    std::mutex dataMutex;
    std::unordered_map<std::string,ProxyHost> ws_proxy_routes;
};

} // namespace proxy
} // namespace network