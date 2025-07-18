#include "connection.h"
#include "proxy/proxy_handler.hpp"
#include "proxy/socks5/socks5_session.h"
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
            asio::buffer(p_data, data_size - kRpcHeadLen),
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
                    auto ret = network::proxy::proxy_handler::make_shared(
                        hostInfo.host, hostInfo.service, std::move(socket_),
                        std::string(ProxyRequest::kVerifyStr, ProxyRequest::kVerifyStrLen));
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

void connection::process_raw_tcp_proxy_request() {
    using network::proxy::ProxyRawTcpRequest;
    do {
        auto data_size = ProxyRawTcpRequest::PeekSize(head_ + 1);
        if (data_size > ProxyRawTcpRequest::kMaxSize) {
            FERR("invalid proxy request size {} overflow {}", data_size, ProxyRawTcpRequest::kMaxSize);
            break;
        }
        if (data_size == 0) {
            FDEBUG("data proxy format error");
            break;
        }
        data_size += ProxyRawTcpRequest::kSizeLen + 1;
        auto proxy_buffer = std::make_shared<std::vector<std::uint8_t>>(data_size);
        std::memcpy(proxy_buffer->data(), head_, kRpcHeadLen);
        auto self(this->shared_from_this());
        auto p_data = proxy_buffer->data() + kRpcHeadLen;
        async_buffer_read(
            asio::buffer(p_data, data_size > kRpcHeadLen ? (data_size - kRpcHeadLen) : 0),
            [this, self, proxy_buffer = std::move(proxy_buffer), data_size](asio::error_code ec, std::size_t length) {
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
                do {
                    ProxyRawTcpRequest request;
                    if (!request.FromBuf(proxy_buffer->data(), proxy_buffer->size())) {
                        FERR("invalid proxy request data");
                        FDEBUG("{}", Fundamental::Utils::BufferToHex(proxy_buffer->data(), proxy_buffer->size()));
                        break;
                    }
                    auto server = this->server_wref_.lock();
                    if (!server) {
                        FERR("server maybe post stop, cancel proxy");
                        break;
                    }
                    FINFO("start raw tcp proxy from {}:{} to {}:{}", get_remote_peer_ip(), get_remote_peer_port(),
                          request.host_, request.service_);
                    std::string pending_data;
                    if (data_size < proxy_buffer->size()) {
                        pending_data =
                            std::string(proxy_buffer->data() + data_size, proxy_buffer->data() + proxy_buffer->size());
                    }
                    auto ret = network::proxy::proxy_handler::make_shared(
                        request.host_, request.service_, std::move(socket_), "", std::move(pending_data));
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

void connection::process_transparent_proxy() {

    auto self(this->shared_from_this());

    // switch to proxy connection,don't need timer check any more
    cancel_timer();
    b_waiting_process_any_data.exchange(false);
    do {
        auto server = this->server_wref_.lock();
        if (!server) {
            FERR("server maybe post stop, cancel proxy");
            break;
        }
        FDEBUG("start raw transparent proxy {}:{} to {}:{}", get_remote_peer_ip(), get_remote_peer_port(),
               external_config.transparent_proxy_host, external_config.transparent_proxy_port);
        auto ret = network::proxy::proxy_handler::make_shared(
            external_config.transparent_proxy_host, external_config.transparent_proxy_port, std::move(socket_), "",
            std::string(head_, head_ + kRpcHeadLen));
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
}
void connection::process_socks5_proxy(const void* preread_data, std::size_t len) {
    auto self(this->shared_from_this());

    // switch to proxy connection,don't need timer check any more
    cancel_timer();
    b_waiting_process_any_data.exchange(false);
    do {
        auto server = this->server_wref_.lock();
        if (!server) {
            FERR("server maybe post stop, cancel proxy");
            break;
        }
        auto ret = SocksV5::Socks5Session::make_shared(executor_, socks5_handler, std::move(socket_));
        ret->start(preread_data, len);
    } while (0);
    release_obj();
}
} // namespace rpc_service
} // namespace network