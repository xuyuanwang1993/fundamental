#include "proxy_handler.hpp"
#include "proxy_codec.hpp"

#include <iostream>

#include "fundamental/basic/log.h"
#include "fundamental/basic/utils.hpp"
namespace network {
namespace proxy {
proxy_handler::proxy_handler(const std::string& proxy_host, const std::string& proxy_service,
                             asio::ip::tcp::socket&& socket) :
proxy_host(proxy_host), proxy_service(proxy_service), socket_(std::move(socket)), proxy_socket_(socket_.get_executor()),
resolver(socket_.get_executor()), cachePool(Fundamental::MakePoolMemorySource()), client2server(cachePool),
server2client(cachePool) {
}

void proxy_handler::SetUp() {
    Process();
}

proxy_handler::~proxy_handler() {
    FDEBUG("~proxy_handler");
    HandleDisconnect({}, "");
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
            if (!callTag.empty()) FDEBUG("{} close proxy remote endpoint", callTag);
        }
    }
    if (closeMask & ProxyDnsResolving) {
        if (status & ProxyDnsResolving) {
            status &= (~ProxyDnsResolving);
            resolver.cancel();
            if (!callTag.empty()) FDEBUG("{} stop proxy dns resolving", callTag);
        }
    }
    if ((closeMask & ServerProxying) || (closeMask & ServerConnecting)) {
        if ((status & ServerProxying) || (status & ServerConnecting)) {
            status &= ~(ServerProxying | ServerConnecting);
            asio::error_code code;
            proxy_socket_.close(code);
            if (!callTag.empty()) FDEBUG("{} close proxy local endpoint", callTag);
        }
    }
}

void proxy_handler::StartDnsResolve(const std::string& host, const std::string& service) {
    FDEBUG("start proxy dns resolve {}:{}", host, service);
    status |= ProxyDnsResolving;
    resolver.async_resolve(
        asio::ip::tcp::v4(), host, service,
        [ref = shared_from_this(), this](asio::error_code ec, decltype(resolver)::results_type result) {
            status ^= ProxyDnsResolving;
            if (ec || result.empty()) {
                HandleDisconnect(ec, "dns resolve");
                return;
            }
            if (status & ClientProxying) StartConnect(std::move(result));
        });
}

void proxy_handler::StartConnect(asio::ip::tcp::resolver::results_type&& result) {
    FDEBUG("start connect to {}:{}  {}:{}", result.begin()->host_name(), result.begin()->service_name(),
           result.begin()->endpoint().address().to_string(), result.begin()->endpoint().port());
    status |= ServerConnecting;
    asio::async_connect(proxy_socket_, result,
                        [this, self = shared_from_this()](std::error_code ec, asio::ip::tcp::endpoint) {
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
                            StartServerRead();
                            StartClient2ServerWrite();
                        });
}

void proxy_handler::HandShake() {
    asio::async_write(socket_, asio::const_buffer(ProxyRequest::kVerifyStr, ProxyRequest::kVerifyStrLen),
                      [this, self = shared_from_this()](std::error_code ec, std::size_t bytesWrite) {
                          if (ec) {
                              HandleDisconnect(ec, "HandShake");
                              return;
                          }
                          FDEBUG("handshake sucess");
                      });
    StartClientRead();
}

void proxy_handler::StartServer2ClientWrite() {
    if (!(status & ClientProxying)) return;
    auto needWrite = server2client.PrepareWriteCache();
    if (needWrite) {
        socket_.async_write_some(server2client.GetWriteBuffer(), [this, self = shared_from_this()](
                                                                     std::error_code ec, std::size_t bytesWrite) {
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
    return asio::buffer(back.data.data() + back.readOffset, kCacheBufferSize - back.readOffset);
}
asio::const_buffer proxy_handler::EndponitCacheStatus::GetWriteBuffer() {
    auto& front = cache_.front();
    return asio::const_buffer(front.data.data() + front.writeOffset, front.readOffset - front.writeOffset);
}
void proxy_handler::EndponitCacheStatus::UpdateReadBuffer(std::size_t readBytes) {
    auto& back = cache_.back();
#ifndef RPC_VERBOSE
    FDEBUG("proxy read {}", Fundamental::Utils::BufferToHex(back.data.data() + back.readOffset, readBytes));
#endif
    back.readOffset += readBytes;
}

void proxy_handler::EndponitCacheStatus::UpdateWriteBuffer(std::size_t writeBytes) {
    auto& front = cache_.front();
#ifndef RPC_VERBOSE
    FDEBUG("proxy write {}", Fundamental::Utils::BufferToHex(front.data.data() + front.writeOffset, writeBytes));
#endif
    front.writeOffset += writeBytes;
}

} // namespace proxy
} // namespace network