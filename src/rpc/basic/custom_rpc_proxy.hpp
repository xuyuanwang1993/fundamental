#pragma once
#include "network/services/proxy_server/traffic_proxy_service/traffic_proxy_codec.hpp"
#include "rpc_client.hpp"
namespace network {
namespace rpc_service {
class CustomRpcProxy : public RpcClientProxyInterface {

public:
    CustomRpcProxy(const std::string& serviceName, const std::string& field, const std::string& token) :
    serviceName_(serviceName), field_(field), token_(token) {
    }

protected:
    std::int32_t FinishSend() override {
        recvBufCache.resize(2);
        return RpcClientProxyInterface::HandShakeStatusMask::HandShakeNeedMoreData;
    }
    std::int32_t FinishRecv() override {
        static const char handshakeStr[] = "ok";
        if (std::memcmp(recvBufCache.data(), handshakeStr, 2) == 0)
            return RpcClientProxyInterface::HandShakeStatusMask::HandShakeSucess;
        else {
            return RpcClientProxyInterface::HandShakeStatusMask::HandShakeFailed;
        }
    }
    void Init() {
        network::proxy::ProxyFrame frame;
        network::proxy::TrafficEncoder::EncodeTrafficProxyRequest(serviceName_, field_, token_, frame);
        frame.ToVecBuffer(sendBufCache);
        curentStatus = RpcClientProxyInterface::HandShakeStatusMask::HandShakeDataPending;
    }

private:
    const std::string serviceName_;
    const std::string field_;
    const std::string token_;
};
} // namespace rpc_service
} // namespace network
