#include "proxy_handler.hpp"
#include "proxy_codec.hpp"

#include <iostream>

#include "fundamental/basic/log.h"
#include "fundamental/basic/utils.hpp"
namespace network
{
namespace proxy
{
proxy_handler::proxy_handler(const std::string& proxy_host,
                             const std::string& proxy_service,
                             asio::ip::tcp::socket&& socket,
                             std::string input_handshake_data) :
proxy_host(proxy_host), proxy_service(proxy_service), socket_(std::move(socket)), proxy_socket_(socket_.get_executor()),
resolver(socket_.get_executor()), handshake_data(input_handshake_data), cachePool(Fundamental::MakePoolMemorySource()),
client2server(cachePool), server2client(cachePool) {
    enable_tcp_keep_alive(socket_);
#ifdef RPC_VERBOSE
    client2server.tag_ = "client2server";
    server2client.tag_ = "server2client";
#endif
}

void proxy_handler::SetUp() {
    Process();
}

proxy_handler::~proxy_handler() {
    FDEBUG("~proxy_handler");
}

void proxy_handler::Process() {
    ProcessTrafficProxy();
}
void proxy_handler::ProcessTrafficProxy() {
    HandShake();
    StartDnsResolve(proxy_host, proxy_service);
}

void proxy_handler::HandleDisconnect(asio::error_code ec, const std::string& callTag, std::int32_t closeMask) {
    if (!callTag.empty()) FDEBUG("disconnect for {} -> ec:{}-{}", callTag, ec.category().name(), ec.message());
    if (closeMask & ClientProxying) {
        if (status & ClientProxying) {
            status &= (~ClientProxying);
            asio::error_code code;
            socket_.close(code);
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

void proxy_handler::StartDnsResolve(const std::string& host, const std::string& service) {
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

void proxy_handler::StartConnect(asio::ip::tcp::resolver::results_type&& result) {
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

void proxy_handler::HandShake() {
    if (!handshake_data.empty()) {
        asio::async_write(socket_, asio::const_buffer(handshake_data.data(), handshake_data.size()),
                          [this, self = shared_from_this()](std::error_code ec, std::size_t bytesWrite) {
                              if (!reference_.is_valid()) {
                                  return;
                              }
                              if (ec) {
                                  HandleDisconnect(ec, "HandShake");
                                  return;
                              }
#ifdef RPC_VERBOSE
                              FDEBUG("proxy handshake sucess");
#endif
                          });
    }

    StartClientRead();
}

void proxy_handler::StartServer2ClientWrite() {
    if (!(status & ClientProxying)) return;
    auto needWrite = server2client.PrepareWriteCache();
    if (needWrite) {
        socket_.async_write_some(server2client.GetWriteBuffer(), [this, self = shared_from_this()](
                                                                     std::error_code ec, std::size_t bytesWrite) {
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

void proxy_handler::StartClientRead() {
    client2server.PrepareReadCache();
    socket_.async_read_some(client2server.GetReadBuffer(),
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

void proxy_handler::StartClient2ServerWrite() {
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

void proxy_handler::StartServerRead() {
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

bool proxy_handler::EndponitCacheStatus::PrepareWriteCache() {
    auto& front = cache_.front();
    // old buffer can be removed
    if (front.readOffset == front.writeOffset && cache_.size() > 1) cache_.pop_front();
    if (cache_.size() == 1) {
        auto& back = cache_.back();
        // last buffer has been written finished
        if (back.readOffset == back.writeOffset) {
            return false;
        }
    }
    // normally we should return true,but we can't call async_write_some twice
    // so we check the flag 'is_writing'
    if (is_writing) return false;
    is_writing = true;
    return true;
}

void proxy_handler::EndponitCacheStatus::PrepareReadCache() {
    do {
        if (cache_.empty()) { // add a new buffer
            cache_.emplace_back();
            break;
        }
        auto& back = cache_.back();
        if (back.writeOffset + kMinPerReadSize > kCacheBufferSize) { // add a new buffer for
            cache_.emplace_back();
            break;
        }
    } while (0);
}
asio::mutable_buffer proxy_handler::EndponitCacheStatus::GetReadBuffer() {
    auto& back = cache_.back();
#ifdef RPC_VERBOSE
    FDEBUG("proxy {} try read {:p} size current_offset:{}", tag_, (void*)&back, back.readOffset);
#endif
    return asio::buffer(back.data.data() + back.readOffset, kCacheBufferSize - back.readOffset);
}
asio::const_buffer proxy_handler::EndponitCacheStatus::GetWriteBuffer() {
    auto& front = cache_.front();
#ifdef RPC_VERBOSE
    FDEBUG("proxy {} try write {:p} size {}-{}={}", tag_, (void*)&front, front.readOffset, front.writeOffset,
           front.readOffset - front.writeOffset);
#endif
    return asio::const_buffer(front.data.data() + front.writeOffset, front.readOffset - front.writeOffset);
}
void proxy_handler::EndponitCacheStatus::UpdateReadBuffer(std::size_t readBytes) {
    if (readBytes == 0) return;
    auto& back = cache_.back();

#ifdef RPC_VERBOSE
    FDEBUG("proxy {} read {:p} {} {} --> {}", tag_, (void*)&back, readBytes, back.readOffset,
           Fundamental::Utils::BufferToHex(back.data.data() + back.readOffset, readBytes, 140));
#endif
    FASSERT(back.readOffset + readBytes <= kCacheBufferSize, " {}+{}<={}", back.readOffset, readBytes,
            kCacheBufferSize);
    back.readOffset += readBytes;
}

void proxy_handler::EndponitCacheStatus::UpdateWriteBuffer(std::size_t writeBytes) {
    is_writing = false;
    if (writeBytes == 0) return;
    auto& front = cache_.front();
#ifdef RPC_VERBOSE
    FDEBUG("proxy {} write {:p} {} {} --> {}", (void*)&front, tag_, writeBytes, front.writeOffset,
           Fundamental::Utils::BufferToHex(front.data.data() + front.writeOffset, writeBytes, 140));
#endif
    FASSERT(front.writeOffset + writeBytes <= front.readOffset, "{}+{} <={}", front.writeOffset, writeBytes,
            front.readOffset);
    front.writeOffset += writeBytes;
}

} // namespace proxy
} // namespace network