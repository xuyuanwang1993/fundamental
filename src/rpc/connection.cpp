#include "connection.h"
#include "proxy/proxy_handler.hpp"
#include "rpc_server.hpp"
namespace network
{
namespace rpc_service
{
std::shared_ptr<ServerStreamReadWriter> connection::InitRpcStream() {
    if (!rpc_stream_rw_) {
        FWARN("it's not a stream rpc call,abort it");
        throw std::logic_error("invalid rpc call with a stream interface");
    }
    std::shared_ptr<ServerStreamReadWriter> ret = rpc_stream_rw_;
    // release server stream write when connection was released
    auto release_handle = reference_.notify_release.Connect([con = ret->weak_from_this()]() {
        auto ptr = con.lock();
        if (ptr) ptr->release_obj();
    });
    // unbind
    ret->reference_.notify_release.Connect([release_handle, s = weak_from_this(), this]() {
        auto ptr = s.lock();
        if (ptr) reference_.notify_release.DisConnect(release_handle);
    });
    ret->start();
    rpc_stream_rw_ = nullptr;
    return ret;
}

void connection::process_proxy_request() {
    using network::proxy::ProxyRequest;
    do {
        if (!proxy_manager_) {
            FDEBUG("data proxy not enabled");
            break;
        }
        auto data_size = ProxyRequest::PeekSize(head_ + 1);
        if (data_size > ProxyRequest::kMaxPayloadLen) {
            FERR("invalid proxy request size {} overflow {}", data_size, ProxyRequest::kMaxPayloadLen);
            break;
        }
        data_size += ProxyRequest::kHeaderLen;
        auto proxy_buffer = std::make_shared<std::vector<std::uint8_t>>(data_size);
        std::memcpy(proxy_buffer->data(), head_, kRpcHeadLen);
        auto self(this->shared_from_this());
        auto p_data = proxy_buffer->data() + kRpcHeadLen;
        async_buffer_read(
            { asio::buffer(p_data, data_size - kRpcHeadLen) },
            [this, self, proxy_buffer = std::move(proxy_buffer)](asio::error_code ec, std::size_t length) {
                if (!socket_.is_open()) {
                    on_net_err_(self, "socket already closed");
                    return;
                }
                if (ec) {
                    FDEBUG("proxy init read failed {}", ec.message());
                    on_net_err_(self, ec.message());
                    release_obj();
                    return;
                }
                // switch to proxy connection,don't need timer check any more
                cancel_timer();
                b_waiting_process_any_data.exchange(false);
#ifdef RPC_VERBOSE
                FDEBUG("server proxy request read {}",
                       Fundamental::Utils::BufferToHex(proxy_buffer->data(), proxy_buffer->size(), 140));
#endif
                do {
                    ProxyRequest request;
                    if (!request.FromBuf(proxy_buffer->data(), proxy_buffer->size())) {
                        FERR("invalid proxy request data");
                        FDEBUG("{}", Fundamental::Utils::BufferToHex(proxy_buffer->data(), proxy_buffer->size()));
                        break;
                    }
                    network::proxy::ProxyHost hostInfo;
                    if (!proxy_manager_->GetProxyHostInfo(request.service_name_, request.token_, request.field_,
                                                          hostInfo)) {
                        FERR("get proxy host failed");
                        break;
                    }
                    auto server = this->server_wref_.lock();
                    if (!server) {
                        FERR("server maybe post stop, cancel proxy");
                        break;
                    }
                    hostInfo.update();
                    FDEBUG("start proxy {} {} {} -> {}:{}", request.service_name_, request.token_, request.field_,
                           hostInfo.host, hostInfo.service);
                    auto ret =
                        network::proxy::proxy_handler::make_shared(hostInfo.host, hostInfo.service, std::move(socket_));
                    // release proxy connection when server was released
                    auto release_handle = server->reference_.notify_release.Connect([con = ret->weak_from_this()]() {
                        auto ptr = con.lock();
                        if (ptr) ptr->release_obj();
                    });
                    // unbind
                    ret->reference_.notify_release.Connect([release_handle, s = server_wref_]() {
                        auto ptr = s.lock();
                        if (ptr) ptr->reference_.notify_release.DisConnect(release_handle);
                    });
                    ret->SetUp();
                } while (0);
                release_obj();
            });
        return;
    } while (0);
    release_obj();
}
} // namespace rpc_service
} // namespace network