#pragma once
#include "fundamental/basic/log.h"
#include "fundamental/basic/utils.hpp"
#include "proxy_codec.hpp"
#include "rpc/basic/rpc_client_proxy.hpp"

namespace network
{
namespace rpc_service
{
class CustomRpcProxy : public RpcClientProxyInterface {
public:
    template <typename... Args>
    static decltype(auto) make_shared(Args&&... args) {
        return std::make_shared<CustomRpcProxy>(std::forward<Args>(args)...);
    }
    CustomRpcProxy(const std::string& serviceName, const std::string& field, const std::string& token) :
    serviceName_(serviceName), field_(field), token_(token) {
    }

protected:
    std::int32_t FinishSend() override {
        FERR("");
        recvBufCache.resize(network::proxy::ProxyRequest::kVerifyStrLen);
        return RpcClientProxyInterface::HandShakeStatusMask::HandShakeNeedMoreData;
    }
    std::int32_t FinishRecv() override {
        FERR("");
        if (std::memcmp(recvBufCache.data(), network::proxy::ProxyRequest::kVerifyStr,
                        network::proxy::ProxyRequest::kVerifyStrLen) == 0)
            return RpcClientProxyInterface::HandShakeStatusMask::HandShakeSucess;
        else {
            return RpcClientProxyInterface::HandShakeStatusMask::HandShakeFailed;
        }
    }
    void Init() {
        network::proxy::ProxyRequest request(serviceName_, token_, field_);
        sendBufCache = request.Encode();
        curentStatus  = RpcClientProxyInterface::HandShakeStatusMask::HandShakeDataPending;
    }

private:
    const std::string serviceName_;
    const std::string field_;
    const std::string token_;
};
} // namespace rpc_service
} // namespace network
