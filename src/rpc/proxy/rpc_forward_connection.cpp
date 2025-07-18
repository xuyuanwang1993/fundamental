#include "rpc_forward_connection.hpp"
#include "rpc/connection.h"
namespace network
{
namespace proxy
{
rpc_forward_connection::rpc_forward_connection(std::shared_ptr<rpc_service::connection> ref_connection,
                                               std::string pre_read_data) :
upstream(ref_connection), proxy_socket_(ref_connection->executor_), resolver(ref_connection->executor_),
cachePool(Fundamental::MakePoolMemorySource()), client2server(cachePool), server2client(cachePool) {
#ifdef RPC_VERBOSE
    client2server.tag_ = "client2server";
    server2client.tag_ = "server2client";
#endif
    client2server.PrepareReadCache();
    // handle pending data
    std::size_t pending_size   = pre_read_data.size();
    std::size_t pending_offset = 0;
    while (pending_size > 0) {
        client2server.PrepareReadCache();
        auto buffer     = client2server.GetReadBuffer();
        auto chunk_size = buffer.size() > pending_size ? pending_size : buffer.size();
        std::memcpy(buffer.data(), pre_read_data.data() + pending_offset, chunk_size);
        client2server.UpdateReadBuffer(chunk_size);
        pending_size -= chunk_size;
        pending_offset += chunk_size;
    }
}

void rpc_forward_connection::start() {
    process_protocal();
}

rpc_forward_connection::~rpc_forward_connection() {
    FDEBUG("~rpc_forward_connection");
}

void rpc_forward_connection::release_obj() {
    reference_.release();
    asio::post(upstream->executor_, [this, ref = shared_from_this()] {
        try {
            HandleDisconnect({}, "release_obj");
        } catch (const std::exception& e) {
        }
    });
}

void rpc_forward_connection::HandleDisconnect(asio::error_code ec, const std::string& callTag, std::int32_t closeMask) {
    if (!callTag.empty()) FDEBUG("disconnect for {} -> ec:{}-{}", callTag, ec.category().name(), ec.message());
    if (closeMask & ClientProxying) {
        if (status & ClientProxying) {
            status &= (~ClientProxying);
            upstream->release_obj();
#ifdef RPC_VERBOSE
            if (!callTag.empty()) FDEBUG("{} close proxy remote endpoint", callTag);
#endif
        }
    }
    if (closeMask & ProxyDnsResolving) {
        if (status & ProxyDnsResolving) {
            status &= (~ProxyDnsResolving);
            resolver.cancel();
#ifdef RPC_VERBOSE
            if (!callTag.empty()) FDEBUG("{} stop proxy dns resolving", callTag);
#endif
        }
    }
    if ((closeMask & ServerProxying) || (closeMask & ServerConnecting)) {
        if ((status & ServerProxying) || (status & ServerConnecting)) {
            status &= ~(ServerProxying | ServerConnecting);
            asio::error_code code;
            proxy_socket_.close(code);
#ifdef RPC_VERBOSE
            if (!callTag.empty()) FDEBUG("{} close proxy local endpoint", callTag);
#endif
        }
    }
}

void rpc_forward_connection::StartDnsResolve(const std::string& host, const std::string& service) {
    FDEBUG("start proxy dns resolve {}:{}", host, service);
    status |= ProxyDnsResolving;
    resolver.async_resolve(
        host, service, [ref = shared_from_this(), this](asio::error_code ec, decltype(resolver)::results_type result) {
            if (!reference_.is_valid()) {
                return;
            }
            status ^= ProxyDnsResolving;
            if (ec || result.empty()) {
                HandleDisconnect(ec, "dns resolve");
                return;
            }
            if (status & ClientProxying) StartConnect(std::move(result));
        });
}

void rpc_forward_connection::StartConnect(asio::ip::tcp::resolver::results_type&& result) {
    status |= ServerConnecting;
    asio::async_connect(proxy_socket_, result,
                        [this, self = shared_from_this()](std::error_code ec, asio::ip::tcp::endpoint endpoint) {
                            if (!reference_.is_valid()) {
                                return;
                            }
                            FDEBUG("proxy connect to {}:{}", endpoint.address().to_string(), endpoint.port());
                            if (ec) {
                                HandleDisconnect(ec, "connect");
                                return;
                            }
                            status ^= ServerConnecting;
                            status |= ServerProxying;
                            if (!(status & ClientProxying)) return;
                            asio::error_code error_code;
                            asio::ip::tcp::no_delay option(true);
                            proxy_socket_.set_option(option, error_code);
                            enable_tcp_keep_alive(proxy_socket_);

                            StartServerRead();
                            StartClient2ServerWrite();
                        });
}

void rpc_forward_connection::StartServer2ClientWrite() {
    if (!(status & ClientProxying)) return;
    auto needWrite = server2client.PrepareWriteCache();
    if (needWrite) {
        upstream->async_write_buffers_some(
            server2client.GetWriteBuffer(),
            [this, self = shared_from_this()](std::error_code ec, std::size_t bytesWrite) {
                if (!reference_.is_valid()) {
                    return;
                }
                if (ec) {
                    HandleDisconnect(ec, "Server2ClientWrite", TrafficProxyCloseExceptServerProxying);
                    return;
                }
                server2client.UpdateWriteBuffer(bytesWrite);
                StartServer2ClientWrite();
            });
    } else { // when server connection is aborted and remaining data to transfer to client
        if (!(status & ServerProxying)) {
            HandleDisconnect({}, "finish client transfer", ClientProxying);
        }
    }
}

void rpc_forward_connection::StartClientRead() {
    client2server.PrepareReadCache();
    upstream->async_buffer_read_some(client2server.GetReadBuffer(),
                                     [this, self = shared_from_this()](std::error_code ec, std::size_t bytesRead) {
                                         if (!reference_.is_valid()) {
                                             return;
                                         }
                                         client2server.UpdateReadBuffer(bytesRead);
                                         Fundamental::ScopeGuard guard([this]() { StartClient2ServerWrite(); });

                                         if (ec) {
                                             HandleDisconnect(ec, "ClientRead", TrafficProxyCloseExceptServerProxying);
                                             return;
                                         }
                                         StartClientRead();
                                     });
}

void rpc_forward_connection::StartClient2ServerWrite() {
    if (!(status & ServerProxying)) return;
    auto needWrite = client2server.PrepareWriteCache();
    if (needWrite) {
        proxy_socket_.async_write_some(client2server.GetWriteBuffer(), [this, self = shared_from_this()](
                                                                           std::error_code ec, std::size_t bytesWrite) {
            if (!reference_.is_valid()) {
                return;
            }
            if (ec) {
                HandleDisconnect(ec, "Client2ServerWrite", TrafficProxyCloseExceptClientProxying);
                return;
            }
            client2server.UpdateWriteBuffer(bytesWrite);
            StartClient2ServerWrite();
        });
    } else { // when client is aborted and remaining data to transfer to server
        if (!(status & ClientProxying)) {
            HandleDisconnect({}, "finish server transfer", ServerProxying);
        }
    }
}

void rpc_forward_connection::StartServerRead() {
    server2client.PrepareReadCache();
    proxy_socket_.async_read_some(server2client.GetReadBuffer(),
                                  [this, self = shared_from_this()](std::error_code ec, std::size_t bytesRead) {
                                      if (!reference_.is_valid()) {
                                          return;
                                      }
                                      server2client.UpdateReadBuffer(bytesRead);
                                      Fundamental::ScopeGuard guard([this]() { StartServer2ClientWrite(); });
                                      if (ec) {
                                          HandleDisconnect(ec, "ServerRead", TrafficProxyCloseExceptClientProxying);
                                          return;
                                      }
                                      StartServerRead();
                                  });
}

} // namespace proxy
} // namespace network