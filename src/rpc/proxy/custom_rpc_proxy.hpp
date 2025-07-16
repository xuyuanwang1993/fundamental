#pragma once
#include "fundamental/basic/log.h"
#include "fundamental/basic/utils.hpp"
#include "network/proxy_interface.hpp"
#include "proxy_codec.hpp"

namespace network
{
namespace rpc_service
{
class CustomRpcProxy : public network_proxy_interface {
public:
    template <typename... Args>
    static decltype(auto) make_shared(Args&&... args) {
        return std::make_shared<CustomRpcProxy>(std::forward<Args>(args)...);
    }
    CustomRpcProxy(const std::string& serviceName, const std::string& field, const std::string& token) :
    serviceName_(serviceName), field_(field), token_(token) {
    }
    const char* interface_name() const override {
        return "custom_rpc_proxy";
    }
    void abort_all_operation() override {
        if (abort_cb_) abort_cb_();
    }
    void start() override {
        FASSERT(read_cb_ && write_cb_ && finish_cb_, "call proxy init first");
        network::proxy::ProxyRequest request(serviceName_, token_, field_);
        sendBufCache = request.Encode();

        process_write();
    }

protected:
    void process_write() {
        network::write_buffer_t write_buffers;
        using value_type = network::write_buffer_t::value_type;
        write_buffers.emplace_back(value_type { sendBufCache.data(), sendBufCache.size() });
        write_cb_(std::move(write_buffers),
                  [this, ptr = shared_from_this()](std::error_code ec, const std::string& msg) {
                      // failed
                      if (ec.value() != kSuccessOpCode) {
                          finish_cb_(ec, msg);
                          return;
                      }
                      process_read();
                  });
    }

    void process_read() {
        recvBufCache.resize(network::proxy::ProxyRequest::kVerifyStrLen);
        network::read_buffer_t read_buffers;
        using value_type = network::read_buffer_t::value_type;
        read_buffers.emplace_back(value_type { recvBufCache.data(), recvBufCache.size() });
        read_cb_(std::move(read_buffers), [this, ptr = shared_from_this()](std::error_code ec, const std::string& msg) {
            // failed
            if (ec.value() != kSuccessOpCode) {
                finish_cb_(ec, msg);
                return;
            }
            std::string finish_msg;
            std::error_code finish_code;
            if (std::memcmp(recvBufCache.data(), network::proxy::ProxyRequest::kVerifyStr,
                            network::proxy::ProxyRequest::kVerifyStrLen) != 0) {
                finish_code = std::make_error_code(std::errc::not_supported);
                finish_msg  = "handshake failed";
            }
            finish_cb_(finish_code, msg);
        });
    }

private:
    const std::string serviceName_;
    const std::string field_;
    const std::string token_;
    std::vector<std::uint8_t> sendBufCache;
    std::vector<std::uint8_t> recvBufCache;
};
} // namespace rpc_service
} // namespace network
