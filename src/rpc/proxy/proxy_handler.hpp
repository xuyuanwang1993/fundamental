#pragma once
#include "proxy_buffer.hpp"
#include "rpc/basic/const_vars.h"
#include <memory>

namespace network
{
namespace rpc_service
{
class connection;
}
namespace proxy
{
class proxy_handler : public std::enable_shared_from_this<proxy_handler> {
    friend class rpc_service::connection;

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
    template <typename... Args>
    static decltype(auto) make_shared(Args&&... args) {
        return std::make_shared<proxy_handler>(std::forward<Args>(args)...);
    }
    explicit proxy_handler(const std::string& proxy_host,
                           const std::string& proxy_service,
                           asio::ip::tcp::socket&& socket,
                           std::string input_handshake_data   = "",
                           std::string pending_data_to_server = "");
    void release_obj() {
        reference_.release();
        asio::post(socket_.get_executor(), [this, ref = shared_from_this()] {
            try {
                HandleDisconnect({}, "release_obj");
            } catch (const std::exception& e) {
            }
        });
    }

protected:
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
    network::network_data_reference reference_;
    std::string proxy_host;
    std::string proxy_service;
    /// Socket for the connection.
    asio::ip::tcp::socket socket_;
    //
    asio::ip::tcp::socket proxy_socket_;
    asio::ip::tcp::resolver resolver;
    std::string handshake_data;
    std::int32_t status = ClientProxying;
    //
    decltype(Fundamental::MakePoolMemorySource()) cachePool;
    EndponitCacheStatus client2server;
    EndponitCacheStatus server2client;
};
} // namespace proxy
} // namespace network