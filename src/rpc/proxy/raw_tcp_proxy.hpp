#pragma once
#include "fundamental/basic/log.h"
#include "fundamental/basic/utils.hpp"
#include "proxy_codec.hpp"
#include "rpc/basic/rpc_client_proxy.hpp"

namespace network
{
namespace rpc_service
{
class RawTcpProxy : public RpcClientProxyInterface {
public:
    template <typename... Args>
    static decltype(auto) make_shared(Args&&... args) {
        return std::make_shared<CustomRpcProxy>(std::forward<Args>(args)...);
    }
    RawTcpProxy(const std::string& host, const std::string& service) : host_(host), service_(service) {
    }

protected:
    std::int32_t FinishSend() override {
        return RpcClientProxyInterface::HandShakeStatusMask::HandShakeSucess;
    }
    std::int32_t FinishRecv() override {
        return RpcClientProxyInterface::HandShakeStatusMask::HandShakeSucess;
    }
    void Init() {
        network::proxy::ProxyRawTcpRequest request(host_, service_);
        sendBufCache = request.Encode();
        curentStatus = RpcClientProxyInterface::HandShakeStatusMask::HandShakeDataPending;
    }

private:
    const std::string host_;
    const std::string service_;
};
} // namespace rpc_service
} // namespace network
