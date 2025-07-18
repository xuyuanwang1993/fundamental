#include "connection.h"
#include "proxy/proxy_handler.hpp"
#include "proxy/socks5/socks5_session.h"
#include "proxy/websocket/ws_forward_connection.hpp"
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

void connection::process_transparent_proxy(std::size_t preread_len) {

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
            std::string(head_, head_ + preread_len));
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

void connection::process_ws_request(std::size_t preread_len) {
    // switch to proxy connection,don't need timer check any more
    cancel_timer();
    b_waiting_process_any_data.exchange(false);
    do {
        auto server = this->server_wref_.lock();
        if (!server) {
            FERR("server maybe post stop, cancel proxy");
            break;
        }
        // std::tuple<bool, std::string, std::string>(std::string)
        auto query_func =
            [this, ptr = shared_from_this()](const std::string& api) -> std::tuple<bool, std::string, std::string> {
            bool ret = false;
            std::string dst_host;
            std::string dst_service;
            do {
                if (!proxy_manager_) break;
                proxy::ProxyHost host;
                ret = proxy_manager_->GetWsProxyRoute(api, host);
                host.update();
                dst_host    = host.host;
                dst_service = host.service;
            } while (0);
            return std::make_tuple(ret, dst_host, dst_service);
        };
        auto ret = network::proxy::websocket_forward_connection::make_shared(
            shared_from_this(), query_func, std::string(head_, head_ + preread_len));
        ret->start();
        // we don't release connection here,forward connection will manage the lifetime of the connection
        return;
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

void connection::probe_protocal(std::size_t offset, std::size_t target_probe_size) {
    FASSERT_ACTION(
        target_probe_size <= kMaxProbeReadSize,
        {
            release_obj();
            return;
        },
        "probe size must < kMaxProbeReadSize");
    auto self(this->shared_from_this());
    if (offset < target_probe_size) {
        async_buffer_read(asio::buffer(head_ + offset, target_probe_size - offset),
                          [this, self, target_probe_size](asio::error_code ec, std::size_t length) {
                              if (!reference_.is_valid()) {
                                  return;
                              }
                              if (!socket_.is_open()) {
                                  on_net_err_(self, "socket already closed");
                                  return;
                              }
                              if (ec) {
#ifdef RPC_VERBOSE
                                  FDEBUG("{:p} rpc read probe head failed {}", (void*)this, ec.message());
#endif
                                  on_net_err_(self, ec.message());
                                  release_obj();
                                  return;
                              }
                              probe_protocal(target_probe_size, target_probe_size);
                          });
    } else {

        switch (head_[0]) {
            // Protocols with packet lengths that may be smaller than the RPC header length (18) should be processed
            // separately.
        case SocksV5::SocksVersion::V5: {
            if (!(external_config.rpc_protocal_mask & network::rpc_protocal_filter_socks5)) goto RPC_FALL_DOWN_ACTION;
            if (!socks5_handler) goto RPC_FALL_DOWN_ACTION;
            process_socks5_proxy(head_, offset);
            break;
        }
        case websocket::kWsMagicNum: {
            if (!(external_config.rpc_protocal_mask & network::rpc_protocal_filter_http_ws)) goto RPC_FALL_DOWN_ACTION;
            process_ws_request(offset);
            // use default handler
            break;
        }

        case RPC_MAGIC_NUM: {
            if (!(external_config.rpc_protocal_mask & network::rpc_protocal_filter_rpc)) goto RPC_FALL_DOWN_ACTION;
            // ensure offset==rpc len
            if (offset < kMaxProbeReadSize) {
                probe_protocal(offset, kMaxProbeReadSize);
                break;
            }
            read_rpc_head(offset);
        } break;
        case network::proxy::ProxyRequest::kMagicNum: {
            if (!(external_config.rpc_protocal_mask & network::rpc_protocal_filter_custom_proxy))
                goto RPC_FALL_DOWN_ACTION;
            // ensure offset==rpc len
            if (offset < kMaxProbeReadSize) {
                probe_protocal(offset, kMaxProbeReadSize);
                break;
            }
            process_proxy_request();

        } break;
        case network::proxy::ProxyRawTcpRequest::kMagicNum: {
            if (!(external_config.rpc_protocal_mask & network::rpc_protocal_filter_raw_tcp_proxy))
                goto RPC_FALL_DOWN_ACTION;
            // ensure offset==rpc len
            if (offset < kMaxProbeReadSize) {
                probe_protocal(offset, kMaxProbeReadSize);
                break;
            }
            process_raw_tcp_proxy_request();
        } break;
        default: goto RPC_FALL_DOWN_ACTION;
        }
        return;
    RPC_FALL_DOWN_ACTION:
        // work as a transparent_proxy，proxy directly
        if (external_config.enable_transparent_proxy) {
            process_transparent_proxy(offset);
            return;
        }
        FERR("none supported protocal");
        release_obj();
    }
}

void connection::read_rpc_head(std::size_t offset) {
    FASSERT(offset <= kRpcHeadLen);
    auto self(this->shared_from_this());
    async_buffer_read(
        asio::buffer(head_ + offset, kRpcHeadLen - offset), [this, self](asio::error_code ec, std::size_t length) {
            if (!reference_.is_valid()) {
                return;
            }
            if (!socket_.is_open()) {
                on_net_err_(self, "socket already closed");
                return;
            }
            if (ec) {
#ifdef RPC_VERBOSE
                FDEBUG("{:p} rpc read head failed {}", (void*)this, ec.message());
#endif
                on_net_err_(self, ec.message());
                release_obj();
                return;
            }
            b_waiting_process_any_data.exchange(false);
#ifdef RPC_VERBOSE
            FDEBUG("server {:p} read head {}", (void*)this, Fundamental::Utils::BufferToHex(head_, kRpcHeadLen));
#endif
            switch (head_[0]) {
            case RPC_MAGIC_NUM: process_rpc_request(); break;
            default: {
                // shoule never reach
                FASSERT(false, "{:p} protocol error magic  {:02x}", (void*)this, static_cast<std::uint8_t>(head_[0]));
                release_obj();
            } break;
            }
        });
}

void connection::read_body(uint32_t func_id, std::size_t size, std::size_t start_offset) {
    auto self(this->shared_from_this());
#ifdef RPC_VERBOSE
    FDEBUG("server {:p} try read size: {}", (void*)this, size - start_offset);
#endif
    async_buffer_read_some(
        asio::mutable_buffer(body_.data() + start_offset, size - start_offset),
        [this, func_id, self, size, start_offset](asio::error_code ec, std::size_t length) {
            if (!reference_.is_valid()) {
                return;
            }
            if (!socket_.is_open()) {
                on_net_err_(self, "socket already closed");
                return;
            }
            if (ec) {
                // do't close socket, wait for write done
                on_net_err_(self, ec.message());
                return;
            }
            b_waiting_process_any_data.exchange(false);
            auto current_read_offset = start_offset + length;
#ifdef RPC_VERBOSE
            FDEBUG("server {:p} read some need:{}  current: {} new:{}", (void*)this, size, current_read_offset, length);
#endif
            if (current_read_offset < size) {
                read_body(func_id, size, current_read_offset);
                return;
            }
#ifdef RPC_VERBOSE
            FDEBUG("server {:p} read {} body {}", (void*)this, size,
                   Fundamental::Utils::BufferToHex(body_.data(), size, 140));
#endif
            read_rpc_head();
            try {
                switch (req_type_) {
                case request_type::rpc_req: process_request(func_id, body_.data(), size); break;
                case request_type::rpc_subscribe: {
                    msgpack_codec codec;
                    auto p              = codec.unpack_tuple<std::tuple<std::string>>(body_.data(), size);
                    auto new_subscriber = std::get<0>(p);
                    auto ret            = subscribers_.insert(new_subscriber);
                    if (ret.second) {
                        on_new_subscriber_added(new_subscriber, weak_from_this());
                    }
                    response_interal(req_id_, msgpack_codec::pack_args_str(result_code::OK, ""));
                } break;
                case request_type::rpc_unsubscribe: {
                    msgpack_codec codec;
                    auto p                  = codec.unpack_tuple<std::tuple<std::string>>(body_.data(), size);
                    auto removed_subscriber = std::get<0>(p);
                    auto iter               = subscribers_.find(removed_subscriber);
                    if (iter != subscribers_.end()) {
                        subscribers_.erase(iter);
                        on_subscribers_removed({ removed_subscriber }, weak_from_this());
                    }
                    response_interal(req_id_, msgpack_codec::pack_args_str(result_code::OK, ""));
                } break;
                case request_type::rpc_publish: {
                    msgpack_codec codec;
                    auto p    = codec.unpack_tuple<std::tuple<std::string, std::string>>(body_.data(), size);
                    auto& key = std::get<0>(p);
                    auto& msg = std::get<1>(p);
                    on_publish_msg(std::move(key), std::move(msg));
                    response_interal(req_id_, msgpack_codec::pack_args_str(result_code::OK, ""));
                } break;
                default: {
                    auto result = msgpack_codec::pack_args_str(
                        result_code::FAIL,
                        Fundamental::StringFormat("bad request type:{}", static_cast<std::int32_t>(req_type_)));
                    response_interal(req_id_, std::move(result));
                } break;
                }
            } catch (const std::exception& ex) {
                auto result = msgpack_codec::pack_args_str(result_code::FAIL, ex.what());
                response_interal(req_id_, std::move(result));
            }
        });
}

void connection::process_rpc_request() {
    rpc_header header;
    header.DeSerialize(head_, kRpcHeadLen);
    req_id_                 = header.req_id;
    const uint32_t body_len = header.body_len;
    req_type_               = header.req_type;
    switch (req_type_) {
    case request_type::rpc_req:
    case request_type::rpc_subscribe:
    case request_type::rpc_unsubscribe:
    case request_type::rpc_publish: {
        if (body_len > 0) {
            if (body_len >= MAX_BUF_LEN) {
                response_interal(req_id_, msgpack_codec::pack_args_str(result_code::FAIL, "body too large"));
                break;
            }
            if (body_.size() < body_len) {
                body_.resize(body_len);
            }
            read_body(header.func_id, body_len);
        } else if (req_type_ == request_type::rpc_req) {
            handle_none_param_request(header.func_id);
        } else {
            response_interal(req_id_, msgpack_codec::pack_args_str(result_code::FAIL, "bad request"));
        }
    } break;
    case request_type::rpc_stream: {
        process_rpc_stream(header.func_id);
    } break;
    case request_type::rpc_heartbeat: {
        read_rpc_head();
        response_interal(req_id_, "", network::rpc_service::request_type::rpc_heartbeat);
        return;
    }
    default: {
        response_interal(req_id_, msgpack_codec::pack_args_str(result_code::FAIL, "bad request type"));
        break;
    }
    }
}
void connection::ssl_handshake() {
    auto self(this->shared_from_this());
    asio::async_read(socket_, asio::buffer(head_, kSslPreReadSize),
                     [this, self](asio::error_code ec, std::size_t length) {
                         if (!reference_.is_valid()) {
                             return;
                         }
                         if (!socket_.is_open()) {
                             on_net_err_(self, "socket already closed");
                             return;
                         }
                         do {
                             if (ec) {
                                 FDEBUG("ssl handshake read failed {}", ec.message());
                                 on_net_err_(self, ec.message());

                                 break;
                             }
                             const std::uint8_t* p_data = (const std::uint8_t*)head_;
                             if (p_data[0] == 0x16) // ssl Handshake
                             {
                                 if (p_data[1] != 0x03 || p_data[2] > 0x03) {
                                     on_net_err_(self, "rpc only support SSL 3.0 to TLS 1.2");
                                     break;
                                 }
                                 do_ssl_handshake(head_, kSslPreReadSize);
                             } else {
                                 // rpc request ssl check
                                 if (!enable_no_ssl_ && p_data[0] == RPC_MAGIC_NUM) {
                                     on_net_err_(self, "only tls connection is supported");
                                     break;
                                 }
#ifdef RPC_VERBOSE
                                 FDEBUG("[rpc] WARNNING!!! falling  down to no ssl socket");
#endif
                                 ssl_context_ref = nullptr;
                                 probe_protocal(kSslPreReadSize);
                             }
                             return;
                         } while (0);
                         release_obj();
                     });
}

void connection::write() {
    if (write_buffers_.empty()) {
        auto& msg   = write_queue_.front();
        write_size_ = (uint32_t)msg.content.size();
        rpc_header { RPC_MAGIC_NUM, msg.req_type, write_size_, msg.req_id, static_cast<std::uint32_t>(conn_id_) }
            .Serialize(write_head_buffer, kRpcHeadLen);

        write_buffers_.emplace_back(asio::buffer(write_head_buffer, kRpcHeadLen));
        if (write_size_ > 0) write_buffers_.emplace_back(asio::buffer(msg.content.data(), write_size_));
    }

    auto self = this->shared_from_this();
    async_write_buffers_some(std::vector<asio::const_buffer>(write_buffers_.begin(), write_buffers_.end()),
                             [this, self](asio::error_code ec, std::size_t length) {
                                 if (!reference_.is_valid()) {
                                     return;
                                 }
                                 if (ec) {
                                     FDEBUG("{:p} rpc write failed {}", (void*)this, ec.message());
                                     on_net_err_(shared_from_this(), ec.message());
                                     release_obj();
                                     return;
                                 }
                                 b_waiting_process_any_data.exchange(false);
#ifdef RPC_VERBOSE
                                 FDEBUG("server {:p} write some size:{}", (void*)this, length);
#endif
                                 while (length != 0) {
                                     if (write_buffers_.empty()) break;
                                     auto current_size = write_buffers_.front().size();
                                     if (length >= current_size) {
                                         length -= current_size;
                                         write_buffers_.pop_front();
                                         continue;
                                     }
                                     write_buffers_.front() = asio::const_buffer(
                                         (std::uint8_t*)write_buffers_.front().data() + length, current_size - length);
                                     break;
                                 }
                                 if (!write_buffers_.empty()) { // write rest data

                                     write();
                                     return;
                                 }
                                 write_queue_.pop_front();

                                 if (!write_queue_.empty()) {
                                     write();
                                 }
                             });
}
} // namespace rpc_service
} // namespace network