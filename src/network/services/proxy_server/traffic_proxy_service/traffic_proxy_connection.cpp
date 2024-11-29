#include "traffic_proxy_connection.hpp"
#include "fundamental/basic/log.h"
#include "fundamental/delay_queue/delay_queue.h"
#include "traffic_proxy_codec.hpp"
#include "traffic_proxy_manager.hpp"
#include <iostream>
namespace network
{
namespace proxy
{
TrafficProxyConnection::TrafficProxyConnection(asio::ip::tcp::socket&& socket, ProxyFrame&& frame) :
ProxeServiceBase(std::move(socket), std::move(frame)),
proxy_socket_(socket_.get_executor()),
resolver(socket_.get_executor()),
checkTimer(socket_.get_executor(), asio::chrono::seconds(s_trafficStatisticsIntervalSec))
{
}

void TrafficProxyConnection::SetUp()
{
    Process();
}

TrafficProxyConnection::~TrafficProxyConnection()
{
    HandleDisconnect({}, "");
}

void TrafficProxyConnection::Process()
{

    using SizeType = decltype(frame.payload.GetSize());
    Fundamental::BufferReader<SizeType> reader;
    reader.SetBuffer(frame.payload.GetData(), frame.payload.GetSize());
    TrafficProxyOperation op;
    try
    {
        reader.ReadValue(&op);
    }
    catch (const std::exception& e)
    {
        FERR("process TrafficProxyConnection op failed {}", e.what());
        return;
    }
    switch (op)
    {
    case TrafficProxyDataOp:
        ProcessTrafficProxy();
        break;

    default:
        FWARN("unsupported traffic proxy op {}", op);
        break;
    }
}

void TrafficProxyConnection::ProcessTrafficProxy()
{
    using SizeType = decltype(frame.payload.GetSize());
    Fundamental::BufferReader<SizeType> reader;
    reader.SetBuffer(frame.payload.GetData() + sizeof(TrafficProxyOperation),
                     frame.payload.GetSize() - sizeof(TrafficProxyOperation));
    TrafficProxyRequest request;
    try
    {
        reader.ReadRawMemory(request.proxyServiceName);
        reader.ReadRawMemory(request.field);
        reader.ReadRawMemory(request.token);
    }
    catch (const std::exception& e)
    {
        FERR("process TrafficProxyConnection TrafficProxyRequest failed {}", e.what());
        return;
    }
    TrafficProxyHost hostInfo;
    if (!TrafficProxyManager::Instance().GetTrafficProxyHostInfo(request.proxyServiceName,
                                                                 request.token,
                                                                 request.field, hostInfo))
    {
        return;
    }
    StartDnsResolve(hostInfo.host.ToString(), hostInfo.service.ToString());
}

void TrafficProxyConnection::HandleDisconnect(asio::error_code ec, const std::string& callTag)
{
    HandleTrafficDataFinished(ec, callTag);
    HandleProxyFinished(ec, callTag);
    StopStatistics();
}

void TrafficProxyConnection::HandleTrafficDataFinished(asio::error_code ec, const std::string& callTag)
{
    if (status & TrafficClientConnected)
        status ^= TrafficClientConnected;
    if (status ^ TrafficClientClosed)
    {
        status |= TrafficClientClosed;
        socket_.close();
    }

    AbortCheck();
}

void TrafficProxyConnection::HandleProxyFinished(asio::error_code ec, const std::string& callTag)
{
    if (status & ProxyConnected)
        status ^= ProxyConnected;
    if (status ^ ProxyClosed)
    {
        status |= ProxyClosed;
        proxy_socket_.close();
    }
    if (status & ProxyDnsResolving)
    {
        status ^= ProxyDnsResolving;
        resolver.cancel();
    }
    AbortCheck();
}

void TrafficProxyConnection::AbortCheck()
{
    do
    {
        if ((status & ProxyConnected) && (status & TrafficClientConnected))
            break;
        if ((status & ProxyConnected) && passive_server_.isWriting)
            break;
        if ((status & TrafficClientConnected) && request_client_.isWriting)
            break;
        StopStatistics();
    } while (0);
}

void TrafficProxyConnection::StartDnsResolve(const std::string& host, const std::string& service)
{
    FDEBUG("start proxy dns resolve {}:{}", host, service);
    status |= ProxyDnsResolving;
    resolver.async_resolve(asio::ip::tcp::v4(),
                           host, service,
                           [ref = shared_from_this(), this](asio::error_code ec,
                                                            decltype(resolver)::results_type result) {
                               status ^= ProxyDnsResolving;
                               if (ec || result.empty())
                               {
                                   HandleProxyFinished(ec, "dns resolve");
                                   return;
                               }
                               StartConnect(std::move(result));
                           });
}

void TrafficProxyConnection::StartConnect(asio::ip::tcp::resolver::results_type&& result)
{
    FDEBUG("start connect to {}:{}  {}:{}",
           result.begin()->host_name(),
           result.begin()->service_name(),
           result.begin()->endpoint().address().to_string(),
           result.begin()->endpoint().port());
    status |= ProxyConnecting;
    asio::async_connect(proxy_socket_, result,
                        [this, self = shared_from_this()](std::error_code ec, asio::ip::tcp::endpoint) {
                            status ^= ProxyConnecting;
                            if (ec)
                            {
                                HandleProxyFinished(ec, "connect");
                                return;
                            }
                            status |= ProxyConnected;
                            StartTrafficClientRead();
                            StartProxyRead();
                            request_client_.InitStatistics();
                            passive_server_.InitStatistics();
                            if (s_trafficStatisticsIntervalSec > 0)
                            {
                                status |= CheckTimerStarted;
                                DoStatistics();
                            }
                        });
}

void TrafficProxyConnection::StartTrafficWrite()
{
    if (status & TrafficClientClosed)
        return;
    passive_server_.PrepareWriteCache();
    if (passive_server_.isWriting)
    {
        socket_.async_write_some(passive_server_.GetWriteBuffer(),
                                 [this, self = shared_from_this()](std::error_code ec, std::size_t bytesWrite) {
                                     if (ec)
                                     {
                                         HandleTrafficDataFinished(ec, "TrafficClientWrite");
                                         return;
                                     }
                                     passive_server_.UpdateWriteBuffer(bytesWrite);
                                     StartTrafficWrite();
                                 });
    }
    else
    {
        if (status & ProxyClosed)
        {
            FDEBUG("traffic proxy finished");
            HandleTrafficDataFinished({}, "");
            StopStatistics();
        }
    }
}

void TrafficProxyConnection::StartTrafficClientRead()
{
    if (status & TrafficClientClosed)
        return;
    request_client_.PrepareReadCache();
    socket_.async_read_some(request_client_.GetReadBuffer(),
                            [this, self = shared_from_this()](std::error_code ec, std::size_t bytesRead) {
                                request_client_.UpdateReadBuffer(bytesRead);
                                Fundamental::ScopeGuard guard([this]() {
                                    StartProxyWrite();
                                });

                                if (ec)
                                {
                                    HandleTrafficDataFinished(ec, "TrafficClientRead");
                                    return;
                                }
                                StartTrafficClientRead();
                            });
}

void TrafficProxyConnection::StartProxyWrite()
{
    if (status & ProxyClosed)
        return;
    request_client_.PrepareWriteCache();
    if (request_client_.isWriting)
    {
        proxy_socket_.async_write_some(request_client_.GetWriteBuffer(),
                                       [this, self = shared_from_this()](std::error_code ec, std::size_t bytesWrite) {
                                           if (ec)
                                           {
                                               HandleProxyFinished(ec, "ProxyWrite");
                                               return;
                                           }
                                           request_client_.UpdateWriteBuffer(bytesWrite);
                                           StartProxyWrite();
                                       });
    }
    else
    {
        if (status & TrafficClientClosed)
        {
            FDEBUG("proxy finished");
            HandleProxyFinished({}, "");
            StopStatistics();
        }
    }
}

void TrafficProxyConnection::StartProxyRead()
{
    if (status & ProxyClosed)
        return;
    passive_server_.PrepareReadCache();
    proxy_socket_.async_read_some(passive_server_.GetReadBuffer(),
                                  [this, self = shared_from_this()](std::error_code ec, std::size_t bytesRead) {
                                      passive_server_.UpdateReadBuffer(bytesRead);
                                      Fundamental::ScopeGuard guard([this]() {
                                          StartTrafficWrite();
                                      });
                                      if (ec)
                                      {
                                          HandleProxyFinished(ec, "ProxyRead");
                                          return;
                                      }
                                      StartProxyRead();
                                  });
}

void TrafficProxyConnection::DoStatistics()
{
    if (!(status & CheckTimerStarted))
        return;
    checkTimer.async_wait([this, self = shared_from_this()](const std::error_code& e) {
        if (e)
        {
            FERR("stop proxy statistics for reason:{}", e.message());
            return;
        }
        request_client_.UpdateStatistics("client");
        passive_server_.UpdateStatistics("proxy");
        DoStatistics();
    });
}

void TrafficProxyConnection::StopStatistics()
{
    if (status & CheckTimerStarted)
    {
        status ^= CheckTimerStarted;
        checkTimer.cancel();
    }
}

void TrafficProxyConnection::EndponitCacheStatus::PrepareWriteCache()
{
    auto& front = cache_.front();
    if (front.readOffset == front.writeOffset && cache_.size() > 1)
        cache_.pop_front();
    if (cache_.size() == 1)
    {
        auto& back = cache_.back();
        if (back.readOffset == back.writeOffset)
        {
            isWriting = false;
            return;
        }
    }
    isWriting = true;
}

void TrafficProxyConnection::EndponitCacheStatus::PrepareReadCache()
{
    do
    {
        if (cache_.empty())
        { // add a new buffer
            cache_.emplace_back();
            break;
        }
        auto& back = cache_.back();
        if (back.writeOffset + kMinPerReadSize > kCacheBufferSize)
        { // add a new buffer for
            cache_.emplace_back();
            break;
        }
    } while (0);
}
asio::mutable_buffer TrafficProxyConnection::EndponitCacheStatus::GetReadBuffer()
{
    auto& back = cache_.back();
    return asio::buffer(back.data.data() + back.readOffset, kCacheBufferSize - back.readOffset);
}
asio::const_buffer TrafficProxyConnection::EndponitCacheStatus::GetWriteBuffer()
{
    auto& front = cache_.front();
    return asio::const_buffer(front.data.data() + front.writeOffset, front.readOffset - front.writeOffset);
}
void TrafficProxyConnection::EndponitCacheStatus::UpdateReadBuffer(std::size_t readBytes)
{
    auto& back = cache_.back();
    back.readOffset += readBytes;
    readBytesNum += readBytes;
}

void TrafficProxyConnection::EndponitCacheStatus::UpdateWriteBuffer(std::size_t writeBytes)
{
    auto& front = cache_.front();
    front.writeOffset += writeBytes;
    writeBytesNum += writeBytes;
}
void TrafficProxyConnection::EndponitCacheStatus::InitStatistics()
{
    lastCheckSecTimePoint = Fundamental::Timer::GetTimeNow<std::chrono::seconds>();
}

void TrafficProxyConnection::EndponitCacheStatus::UpdateStatistics(const std::string& tag)
{
    auto timePoint = Fundamental::Timer::GetTimeNow<std::chrono::seconds>();
    if (timePoint == lastCheckSecTimePoint)
        return;
    auto timeDiff           = timePoint - lastCheckSecTimePoint;
    auto readBytesDiff      = (readBytesNum - lastReadBytesNum) / 1024.0;
    auto writeBytesDiff     = (writeBytesNum - lastWriteBytesNum) / 1024.0;
    auto readSpeedKBPerSec  = readBytesDiff / timeDiff;
    auto writeSpeedKBPerSec = writeBytesDiff / timeDiff;
    // update
    lastCheckSecTimePoint = timePoint;
    lastReadBytesNum      = readBytesNum;
    lastWriteBytesNum     = writeBytesNum;
    FDEBUG("{} read {}kB/s->{}bytes write:{}kB/s->{}bytes", tag, readSpeedKBPerSec,
           lastReadBytesNum, writeSpeedKBPerSec, lastWriteBytesNum);
}
} // namespace proxy
} // namespace network