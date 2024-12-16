#pragma once
#include "network/services/proxy_server/proxy_request_handler.hpp"
#include "traffic_proxy_defines.h"

#include <array>
#include <asio.hpp>
#include <deque>
#include <memory>
namespace network
{
namespace proxy
{
class TrafficProxyConnection : public ProxeServiceBase, public std::enable_shared_from_this<TrafficProxyConnection>
{
    inline static constexpr std::size_t kCacheBufferSize     = 32 * 1024; // 32k
    inline static constexpr std::size_t kMinPerReadSize      = 1200;
    inline static std::size_t s_trafficStatisticsIntervalSec = 2;
    using DataCacheType                                      = std::array<std::uint8_t, kCacheBufferSize>;
    struct DataCahceItem
    {
        DataCacheType data;
        std::size_t readOffset  = 0;
        std::size_t writeOffset = 0;
    };

    struct EndponitCacheStatus
    {
        std::deque<DataCahceItem> cache_;
        bool isWriting = false;
        // Statistics
        std::size_t writeBytesNum = 0;
        std::size_t readBytesNum  = 0;
        //
        std::int64_t lastCheckSecTimePoint = 0;
        std::size_t lastReadBytesNum       = 0;
        std::size_t lastWriteBytesNum      = 0;
        void PrepareWriteCache();
        void PrepareReadCache();
        asio::mutable_buffer GetReadBuffer();
        asio::const_buffer GetWriteBuffer();
        void UpdateReadBuffer(std::size_t readBytes);
        void UpdateWriteBuffer(std::size_t writeBytes);
        //
        void InitStatistics();
        void UpdateStatistics(const std::string& tag);
    };
    enum TrafficProxyStatusMask : std::int32_t
    {
        ClientProxying                        = (1 << 0),
        CheckTimerHandling                    = (1 << 1),
        ProxyDnsResolving                     = (1 << 2),
        ServerProxying                        = (1 << 3),
        ServerConnecting                      = (1 << 4),
        TrafficProxyCloseExceptClientProxying = static_cast<std::int32_t>(~ClientProxying),
        TrafficProxyCloseExceptServerProxying = static_cast<std::int32_t>(~ServerProxying),
        TrafficProxyCloseAll                  = static_cast<std::int32_t>(~0),
    };

public:
    void SetUp() override;
    ~TrafficProxyConnection();
    static std::shared_ptr<TrafficProxyConnection> MakeShared(asio::ip::tcp::socket&& socket, ProxyFrame&& frame)
    {
        return std::shared_ptr<TrafficProxyConnection>(new TrafficProxyConnection(std::move(socket), std::move(frame)));
    }

protected:
    explicit TrafficProxyConnection(asio::ip::tcp::socket&& socket, ProxyFrame&& frame);
    void Process();
    void ProcessTrafficProxy();
    void HandleDisconnect(asio::error_code ec, const std::string& callTag = "",std::int32_t closeMask=TrafficProxyCloseAll);

protected:
    void StartDnsResolve(const std::string& host, const std::string& service);
    void StartConnect(asio::ip::tcp::resolver::results_type&& result);
    void HandShake();
    void StartServer2ClientWrite();
    void StartClientRead();
    void StartClient2ServerWrite();
    void StartServerRead();
    void DoStatistics();

protected:
    asio::ip::tcp::socket proxy_socket_;
    asio::ip::tcp::resolver resolver;
    asio::steady_timer checkTimer;
    char handshakeBuf[2];
    std::int32_t status = ClientProxying;
    EndponitCacheStatus client2server;
    EndponitCacheStatus server2client;
};
} // namespace proxy
} // namespace network