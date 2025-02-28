#pragma once

#include <array>
#include <asio.hpp>
#include <deque>
#include <memory>

#include "fundamental/basic/allocator.hpp"
namespace network
{
namespace proxy
{
class proxy_handler : public std::enable_shared_from_this<proxy_handler> {
    inline static constexpr std::size_t kCacheBufferSize = 32 * 1024; // 32k
    inline static constexpr std::size_t kMinPerReadSize  = 1200;
    using DataCacheType                                  = std::array<std::uint8_t, kCacheBufferSize>;
    struct DataCahceItem {
        DataCacheType data;
        std::size_t readOffset  = 0;
        std::size_t writeOffset = 0;
    };

    struct EndponitCacheStatus {
        explicit EndponitCacheStatus(decltype(Fundamental::MakePoolMemorySource()) dataSource) :
        cache_(dataSource.get()) {
        }
        bool is_writing = false;
        std::deque<DataCahceItem, Fundamental::AllocatorType<DataCahceItem>> cache_;
        std::string tag_;
        bool PrepareWriteCache();
        void PrepareReadCache();
        asio::mutable_buffer GetReadBuffer();
        asio::const_buffer GetWriteBuffer();
        void UpdateReadBuffer(std::size_t readBytes);
        void UpdateWriteBuffer(std::size_t writeBytes);
    };
    enum TrafficProxyStatusMask : std::int32_t
    {
        ClientProxying                        = (1 << 0),
        ProxyDnsResolving                     = (1 << 2),
        ServerProxying                        = (1 << 3),
        ServerConnecting                      = (1 << 4),
        TrafficProxyCloseExceptClientProxying = static_cast<std::int32_t>(~ClientProxying),
        TrafficProxyCloseExceptServerProxying = static_cast<std::int32_t>(~ServerProxying),
        TrafficProxyCloseAll                  = static_cast<std::int32_t>(~0),
    };

public:
    void SetUp();
    ~proxy_handler();
    static std::shared_ptr<proxy_handler> MakeShared(const std::string& proxy_host,
                                                     const std::string& proxy_service,
                                                     asio::ip::tcp::socket&& socket) {
        return std::shared_ptr<proxy_handler>(new proxy_handler(proxy_host, proxy_service, std::move(socket)));
    }

protected:
    explicit proxy_handler(const std::string& proxy_host,
                           const std::string& proxy_service,
                           asio::ip::tcp::socket&& socket);
    void Process();
    void ProcessTrafficProxy();
    void HandleDisconnect(asio::error_code ec,
                          const std::string& callTag = "",
                          std::int32_t closeMask     = TrafficProxyCloseAll);

protected:
    void StartDnsResolve(const std::string& host, const std::string& service);
    void StartConnect(asio::ip::tcp::resolver::results_type&& result);
    void HandShake();
    void StartServer2ClientWrite();
    void StartClientRead();
    void StartClient2ServerWrite();
    void StartServerRead();

protected:
    const std::string proxy_host;
    const std::string proxy_service;
    /// Socket for the connection.
    asio::ip::tcp::socket socket_;
    //
    asio::ip::tcp::socket proxy_socket_;
    asio::ip::tcp::resolver resolver;
    char handshakeBuf[2];
    std::int32_t status = ClientProxying;
    //
    decltype(Fundamental::MakePoolMemorySource()) cachePool;
    EndponitCacheStatus client2server;
    EndponitCacheStatus server2client;
};
} // namespace proxy
} // namespace network