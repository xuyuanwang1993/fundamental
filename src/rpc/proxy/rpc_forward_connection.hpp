#pragma once
#include "proxy_buffer.hpp"
#include "rpc/basic/const_vars.h"
#include <memory>

#include "rpc/connection.h"
namespace network
{
namespace proxy
{
class rpc_forward_connection : public std::enable_shared_from_this<rpc_forward_connection> {
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
    void start();
    virtual ~rpc_forward_connection();
    explicit rpc_forward_connection(std::shared_ptr<rpc_service::connection> ref_connection,
                                    std::string pre_read_data = "");
    void release_obj();

protected:
    virtual void process_protocal() = 0;
    void HandleDisconnect(asio::error_code ec,
                          const std::string& callTag = "",
                          std::int32_t closeMask     = TrafficProxyCloseAll);

protected:
    void StartDnsResolve(const std::string& host, const std::string& service);
    void StartConnect(asio::ip::tcp::resolver::results_type&& result);
    void StartServer2ClientWrite();
    void StartClientRead();
    void StartClient2ServerWrite();
    void StartServerRead();

protected:
    // forward imp
    template <typename Handler, typename BufferType>
    inline void forward_async_buffer_read(BufferType buffers, Handler handler) {
        upstream->async_buffer_read(std::move(buffers), handler);
    }

    template <typename Handler, typename BufferType>
    inline void forward_async_buffer_read_some(BufferType buffers, Handler handler) {
        upstream->async_buffer_read_some(std::move(buffers), handler);
    }

    template <typename BufferType, typename Handler>
    inline void forward_async_write_buffers(BufferType buffers, Handler handler) {
        upstream->async_write_buffers(std::move(buffers), handler);
    }
    template <typename BufferType, typename Handler>
    inline void forward_async_write_buffers_some(BufferType&& buffers, Handler handler) {
        upstream->async_write_buffers_some(std::move(buffers), handler);
    }

protected:
    network::network_data_reference reference_;
    std::string proxy_host;
    std::string proxy_service;
    /// Socket for the connection.
    std::shared_ptr<rpc_service::connection> upstream;
    //
    asio::ip::tcp::socket proxy_socket_;
    asio::ip::tcp::resolver resolver;
    std::int32_t status = ClientProxying;
    //
    decltype(Fundamental::MakePoolMemorySource()) cachePool;
    EndponitCacheStatus client2server;
    EndponitCacheStatus server2client;
};

} // namespace proxy
} // namespace network