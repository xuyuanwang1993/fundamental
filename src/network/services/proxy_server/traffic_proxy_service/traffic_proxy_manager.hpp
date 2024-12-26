#pragma once
#include "traffic_proxy_defines.h"
#include <mutex>

namespace network {
namespace proxy {

class TrafficProxyManager : public Fundamental::Singleton<TrafficProxyManager> {
public:
    inline static constexpr const char* kDefaultFieldName = "default";

public:
    void UpdateTrafficProxyHostInfo(const TrafficProxyDataType& serviceName, TrafficProxyHostInfo&& hostInfo);
    void RemoveTrafficProxyHostInfo(const TrafficProxyDataType& serviceName);
    bool GetTrafficProxyHostInfo(const TrafficProxyDataType& serviceName, const TrafficProxyDataType& token,
                                 const TrafficProxyDataType& field, TrafficProxyHost& outHost);

private:
    std::mutex dataMutex;
    TrafficProxyHostMap storage;
};

} // namespace proxy
} // namespace network