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
    inline static constexpr const char* kDefaultFieldName = "default";

public:
    virtual ~ProxyManager() = default;
    virtual void UpdateProxyHostInfo(const std::string& serviceName, ProxyHostInfo&& hostInfo);
    virtual void RemoveProxyHostInfo(const std::string& serviceName);
    virtual bool GetProxyHostInfo(const std::string& serviceName,
                                  const std::string& token,
                                  const std::string& field,
                                  ProxyHost& outHost);

    virtual void AddWsProxyRoute(const std::string& api_route, ProxyHost host);
    virtual void RemoveWsProxyRoute(const std::string& api_route);
    virtual bool GetWsProxyRoute(const std::string& api_route, ProxyHost& host);

private:
    std::mutex dataMutex;
    ProxyHostMap storage;
    std::unordered_map<std::string,ProxyHost> ws_proxy_routes;
};

} // namespace proxy
} // namespace network