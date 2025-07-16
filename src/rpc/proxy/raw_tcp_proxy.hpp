#pragma once
#include "fundamental/basic/log.h"
#include "fundamental/basic/utils.hpp"
#include "network/proxy_interface.hpp"
#include "proxy_codec.hpp"

namespace network
{
namespace rpc_service
{
class RawTcpProxy : public network_proxy_interface {
public:
    template <typename... Args>
    static decltype(auto) make_shared(Args&&... args) {
        return std::make_shared<RawTcpProxy>(std::forward<Args>(args)...);
    }
    RawTcpProxy(const std::string& host, const std::string& service) : host_(host), service_(service) {
    }
    void abort_all_operation() override {
        if (abort_cb_) abort_cb_();
    }
    void start() override {
        FASSERT(read_cb_ && write_cb_ && finish_cb_, "call proxy init first");
        network::proxy::ProxyRawTcpRequest request(host_, service_);
        sendBufCache = request.Encode();

        process_write();
    }
    const char* interface_name() const override {
        return "raw_tcp_proxy";
    }

protected:
    void process_write() {
        network::write_buffer_t write_buffers;
        using value_type = network::write_buffer_t::value_type;
        write_buffers.emplace_back(value_type { sendBufCache.data(), sendBufCache.size() });
        write_cb_(std::move(write_buffers), finish_cb_);
    }

private:
    const std::string host_;
    const std::string service_;
    std::vector<std::uint8_t> sendBufCache;
};
} // namespace rpc_service
} // namespace network
