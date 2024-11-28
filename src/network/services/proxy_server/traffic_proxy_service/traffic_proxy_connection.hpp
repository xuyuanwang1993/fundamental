#pragma once
#include "network/services/proxy_server/proxy_request_handler.hpp"
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
        std::int64_t lastCheckSecTimePoint          = 0;
        std::size_t lastReadBytesNum                = 0;
        std::size_t lastWriteBytesNum               = 0;
        void PrepareWriteCache();
        void PrepareReadCache();
        asio::mutable_buffer GetReadBuffer();
        asio::const_buffer GetWriteBuffer();
        void UpdateReadBuffer(std::size_t readBytes);
        void UpdateWriteBuffer(std::size_t writeBytes);
        //
        void InitStatistics();
        void UpdateStatistics(const std::string &tag);
    };

public:
    explicit TrafficProxyConnection(asio::ip::tcp::socket&& socket, ProxyFrame&& frame);
    void SetUp() override;
    ~TrafficProxyConnection();

protected:
    void Process();
    void ProcessTrafficProxy();
    void HandleDisconnect(asio::error_code ec, const std::string& callTag = "");

protected:
    void StartDnsResolve(const std::string& host, const std::string& service);
    void StartConnect(asio::ip::tcp::resolver::results_type&& result);
    void StartTrafficWrite();
    void StartTrafficClientRead();
    void StartProxyWrite();
    void StartProxyRead();
    void StartStatistics();

protected:
    asio::ip::tcp::socket proxy_socket_;
    asio::ip::tcp::resolver resolver;
    asio::steady_timer checkTimer;

    EndponitCacheStatus request_client_;
    EndponitCacheStatus passive_server_;
};
} // namespace proxy
} // namespace network