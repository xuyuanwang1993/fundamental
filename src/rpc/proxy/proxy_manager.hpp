#pragma once
#include "proxy_defines.h"
#include <mutex>

#include "fundamental/basic/utils.hpp"

namespace network {
namespace proxy {

class ProxyManager {
public:
    inline static constexpr const char* kDefaultFieldName = "default";

public:
    virtual ~ProxyManager() = default;
    virtual void UpdateProxyHostInfo(const std::string& serviceName, ProxyHostInfo&& hostInfo);
    virtual void RemoveProxyHostInfo(const std::string& serviceName);
    virtual bool GetProxyHostInfo(const std::string& serviceName, const std::string& token, const std::string& field,
                                  ProxyHost& outHost);

private:
    std::mutex dataMutex;
    ProxyHostMap storage;
};

} // namespace proxy
} // namespace network