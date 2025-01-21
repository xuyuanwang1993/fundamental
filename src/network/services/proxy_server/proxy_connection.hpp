#pragma once
#include "network/server/basic_server.hpp"
#include "proxy_defines.h"
#include "proxy_request_handler.hpp"

#include <array>
#include <asio.hpp>
#include <deque>
#include <memory>
#include <vector>

#include "fundamental/events/event_system.h"

namespace network {
namespace proxy {

/// Represents a single Connection from a client.
class Connection : public ConnectionInterface<ProxyRequestHandler>, public std::enable_shared_from_this<Connection> {
    static constexpr std::size_t kMaxRecvRequestFrameTimeSec = 30;
    friend class ProxyRequestHandler;

public:
    /// Construct a Connection with the given socket.
    explicit Connection(asio::ip::tcp::socket socket, ProxyRequestHandler& handler);
    ~Connection();
    /// Start the first asynchronous operation for the Connection.
    void Start() override;

private:
    void HandleClose();
    void ReadHeader();
    void ReadBody();
    void StartTimerCheck();
    void StopTimeCheck();

private:
    std::array<std::uint8_t, ProxyFrame::kHeaderSize> headerBuffer;
    ProxyFrame requestFrame;
    asio::steady_timer checkTimer;
};

using ProxyServer = network::Server<Connection, ProxyRequestHandler>;

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    // this signals will not be blocked
    Fundamental::Signal<void()> DnsResloveStarted;
    Fundamental::Signal<void(const asio::error_code& ec, const asio::ip::tcp::resolver::results_type& result)>
        DnsResloveFinished;
    Fundamental::Signal<void(const asio::ip::tcp::resolver::results_type& endpoint)> ConnectStarted;
    Fundamental::Signal<void(const std::error_code& ec, const asio::ip::tcp::endpoint& endpoint)> ConnectFinished;

public:
    /// @note you should ensure all clientsession instance was be managered by std::shared_ptr
    static decltype(auto) MakeShared(const std::string& host, const std::string& service) {
        return std::shared_ptr<ClientSession>(new ClientSession(host, service));
    }
    /// @brief start client session task
    virtual void Start();
    /// @brief  abort all client operation,this function just post a abort request
    virtual void Abort();
    virtual ~ClientSession() = default;
    /// @brief if you want connect any signals after RpcCall function has been called
    /// you should call AcquireInitLocker earlier than calling RpcCall function to block signal calls
    void AcquireInitLocker();
    /// @brief call this function when you have done all initialization operations
    void ReleaseInitLocker();

protected:
    virtual void StartDnsResolve();
    virtual void FinishDnsResolve(asio::error_code ec, asio::ip::tcp::resolver::results_type result);
    virtual void StartConnect(asio::ip::tcp::resolver::results_type result);
    virtual void FinishConnect(std::error_code ec, asio::ip::tcp::endpoint endpoint);
    /// @brief  do something while connection success
    virtual void Process(asio::ip::tcp::endpoint endpoint);
    // cancel socket operation and dns operation
    virtual void Cancel();
    virtual void HandelFailed(std::error_code ec);
    /// @brief return true means session operation has already been aborted,it will call HandelFailed
    /// with an error code request_aborted
    virtual bool AbortCheckPoint();

protected:
    explicit ClientSession(const std::string& host, const std::string& service);

protected:
    const std::string host;
    const std::string service;
    asio::ip::tcp::socket socket_;
    asio::ip::tcp::resolver resolver;
    std::atomic_bool is_aborted_ = false;
    std::atomic_flag init_fence_ = ATOMIC_FLAG_INIT;
};
} // namespace proxy
} // namespace network