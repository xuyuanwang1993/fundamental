#include "traffic_proxy_connection.hpp"
#include "fundamental/basic/log.h"
#include "fundamental/delay_queue/delay_queue.h"
#include "traffic_proxy_codec.hpp"
#include "traffic_proxy_manager.hpp"
#include <iostream>
namespace network {
namespace proxy {
TrafficProxyConnection::TrafficProxyConnection(asio::ip::tcp::socket&& socket, ProxyFrame&& frame) :
ProxeServiceBase(std::move(socket), std::move(frame)), proxy_socket_(socket_.get_executor()),
resolver(socket_.get_executor()),
checkTimer(socket_.get_executor(), asio::chrono::seconds(s_trafficStatisticsIntervalSec)),
cachePool(Fundamental::MakePoolMemorySource()), client2server(cachePool), server2client(cachePool) {
}

void TrafficProxyConnection::SetUp() {
    Process();
}

TrafficProxyConnection::~TrafficProxyConnection() {
    FDEBUG("~TrafficProxyConnection");
    HandleDisconnect({}, "");
}

void TrafficProxyConnection::Process() {
    ProcessTrafficProxy();
}

void TrafficProxyConnection::ProcessTrafficProxy() {
    TrafficProxyRequest request;
    if (!TrafficDecoder::DecodeCommandFrame(frame.payload, request)) {
        return;
    }
    TrafficProxyHost hostInfo;
    if (!TrafficProxyManager::Instance().GetTrafficProxyHostInfo(request.proxyServiceName, request.token, request.field,
                                                                 hostInfo)) {
        return;
    }
    FDEBUG("start proxy {} {} {} -> {}:{}", request.proxyServiceName.ToString(), request.token.ToString(),
           request.field.ToString(), hostInfo.host.ToString(), hostInfo.service.ToString());
    HandShake();
    StartDnsResolve(hostInfo.host.ToString(), hostInfo.service.ToString());
}

void TrafficProxyConnection::HandleDisconnect(asio::error_code ec, const std::string& callTag, std::int32_t closeMask) {
    if (!callTag.empty()) FDEBUG("disconnect for {} -> ec:{}-{}", callTag, ec.category().name(), ec.message());
    if (closeMask & ClientProxying) {
        if (status & ClientProxying) {
            status ^= ClientProxying;
            asio::error_code code;
            socket_.close(code);
            if (!callTag.empty()) FDEBUG("close proxy remote endpoint ");
        }
    }
    if (closeMask & CheckTimerHandling) {
        if (status & CheckTimerHandling) {
            status ^= CheckTimerHandling;
            checkTimer.cancel();
            if (!callTag.empty()) FDEBUG("stop proxy statistics");
        }
    }
    if (closeMask & ProxyDnsResolving) {
        if (status & ProxyDnsResolving) {
            status ^= ProxyDnsResolving;
            resolver.cancel();
            if (!callTag.empty()) FDEBUG("stop proxy dns resolving");
        }
    }
    if ((closeMask & ServerProxying) || (closeMask & ServerConnecting)) {
        if ((status & ServerProxying) || (status & ServerConnecting)) {
            status &= (~ServerProxying);
            status &= (~ServerConnecting);
            asio::error_code code;
            proxy_socket_.close(code);
            if (!callTag.empty()) FDEBUG("close proxy local endpoint");
        }
    }
}

void TrafficProxyConnection::StartDnsResolve(const std::string& host, const std::string& service) {
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

void TrafficProxyConnection::StartConnect(asio::ip::tcp::resolver::results_type&& result) {
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
                            server2client.InitStatistics();
                            if (s_trafficStatisticsIntervalSec > 0) {
                                status |= CheckTimerHandling;
                                DoStatistics();
                            }
                        });
}

void TrafficProxyConnection::HandShake() {
    handshakeBuf[0] = 'o';
    handshakeBuf[1] = 'k';
    asio::async_write(socket_, asio::const_buffer(handshakeBuf, 2),
                      [this, self = shared_from_this()](std::error_code ec, std::size_t bytesWrite) {
                          if (ec) {
                              HandleDisconnect(ec, "HandShake");
                              return;
                          }
                          FDEBUG("handshake sucess");
                      });
    StartClientRead();
    client2server.InitStatistics();
}

void TrafficProxyConnection::StartServer2ClientWrite() {
    if (!(status & ClientProxying)) return;
    auto needWrite = server2client.PrepareWriteCache();
    if (needWrite) {
        socket_.async_write_some(server2client.GetWriteBuffer(), [this, self = shared_from_this()](
                                                                     std::error_code ec, std::size_t bytesWrite) {
            server2client.ClearWriteStatus();
            if (ec) {
                HandleDisconnect(ec, "Server2ClientWrite", TrafficProxyCloseExceptServerProxying);
                return;
            }
            server2client.UpdateWriteBuffer(bytesWrite);
            StartServer2ClientWrite();
        });
    } else

    { // when server connection is aborted and remaining data to transfer to client
        if (!(status & ServerProxying)) {
            HandleDisconnect({}, "finish client transfer", ClientProxying);
        }
    }
}

void TrafficProxyConnection::StartClientRead() {
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

void TrafficProxyConnection::StartClient2ServerWrite() {
    if (!(status & ServerProxying)) return;
    auto needWrite = client2server.PrepareWriteCache();
    if (needWrite) {
        proxy_socket_.async_write_some(client2server.GetWriteBuffer(), [this, self = shared_from_this()](
                                                                           std::error_code ec, std::size_t bytesWrite) {
            client2server.ClearWriteStatus();
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

void TrafficProxyConnection::StartServerRead() {
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

void TrafficProxyConnection::DoStatistics() {
    if (!(status & CheckTimerHandling)) return;
    checkTimer.expires_after(asio::chrono::seconds(s_trafficStatisticsIntervalSec));
    checkTimer.async_wait([this, self = shared_from_this()](const std::error_code& e) {
        if (e) {
            FDEBUG("stop proxy statistics for reason:{}", e.message());
            return;
        }
        client2server.UpdateStatistics("client");
        server2client.UpdateStatistics("proxy");
        DoStatistics();
    });
}

void TrafficProxyConnection::EndponitCacheStatus::ClearWriteStatus() {
    isWriting = false;
}

bool TrafficProxyConnection::EndponitCacheStatus::PrepareWriteCache() {
    auto oldStatus = isWriting;
    auto& front    = cache_.front();
    if (front.readOffset == front.writeOffset && cache_.size() > 1) cache_.pop_front();
    if (cache_.size() == 1) {
        auto& back = cache_.back();
        if (back.readOffset == back.writeOffset) {
            isWriting = false;
            return false;
        }
    }
    isWriting = true;
    return !oldStatus && isWriting;
}

void TrafficProxyConnection::EndponitCacheStatus::PrepareReadCache() {
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
asio::mutable_buffer TrafficProxyConnection::EndponitCacheStatus::GetReadBuffer() {
    auto& back = cache_.back();
    return asio::buffer(back.data.data() + back.readOffset, kCacheBufferSize - back.readOffset);
}
asio::const_buffer TrafficProxyConnection::EndponitCacheStatus::GetWriteBuffer() {
    auto& front = cache_.front();
    return asio::const_buffer(front.data.data() + front.writeOffset, front.readOffset - front.writeOffset);
}
void TrafficProxyConnection::EndponitCacheStatus::UpdateReadBuffer(std::size_t readBytes) {
    auto& back = cache_.back();
    back.readOffset += readBytes;
    readBytesNum += readBytes;
}

void TrafficProxyConnection::EndponitCacheStatus::UpdateWriteBuffer(std::size_t writeBytes) {
    auto& front = cache_.front();
    front.writeOffset += writeBytes;
    writeBytesNum += writeBytes;
}
void TrafficProxyConnection::EndponitCacheStatus::InitStatistics() {
    lastCheckSecTimePoint = Fundamental::Timer::GetTimeNow<std::chrono::seconds>();
}

void TrafficProxyConnection::EndponitCacheStatus::UpdateStatistics(const std::string& tag) {
    auto timePoint = Fundamental::Timer::GetTimeNow<std::chrono::seconds>();
    if (timePoint == lastCheckSecTimePoint) return;
    auto timeDiff           = timePoint - lastCheckSecTimePoint;
    auto readBytesDiff      = (readBytesNum - lastReadBytesNum) / 1024.0;
    auto writeBytesDiff     = (writeBytesNum - lastWriteBytesNum) / 1024.0;
    auto readSpeedKBPerSec  = readBytesDiff / timeDiff;
    auto writeSpeedKBPerSec = writeBytesDiff / timeDiff;
    // update
    lastCheckSecTimePoint = timePoint;
    lastReadBytesNum      = readBytesNum;
    lastWriteBytesNum     = writeBytesNum;
    FDEBUG("{} read {}kB/s->{}bytes write:{}kB/s->{}bytes", tag, readSpeedKBPerSec, lastReadBytesNum,
           writeSpeedKBPerSec, lastWriteBytesNum);
}
} // namespace proxy
} // namespace network