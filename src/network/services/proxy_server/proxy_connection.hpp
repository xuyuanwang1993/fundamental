#pragma once
#include "network/server/basic_server.hpp"
#include "proxy_defines.h"
#include "proxy_request_handler.hpp"

#include <array>
#include <asio.hpp>
#include <deque>
#include <memory>
#include <vector>
namespace network
{
namespace proxy
{

/// Represents a single Connection from a client.
class Connection
: public ConnectionInterface<ProxyRequestHandler>,
  public std::enable_shared_from_this<Connection>
{
    static constexpr std::size_t kMaxRecvRequestFrameTimeSec = 30;
    friend struct ProxyRequestHandler;

public:
    /// Construct a Connection with the given socket.
    explicit Connection(asio::ip::tcp::socket socket,
                        ProxyRequestHandler& handler);

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

class ClientSession :public std::enable_shared_from_this<ClientSession>
{
public:
    /// @note you should ensure all clientsession instance was be managered by std::shared_ptr
    static decltype(auto) MakeShared(const std::string& host, const std::string& service)
    {
        return std::shared_ptr<ClientSession>(new ClientSession(host, service));
    }
    /// @brief start client session task
    virtual void Start();
    /// @brief  abort all client operation,this function just post a abort request
    virtual void Abort();
    virtual ~ClientSession() = default;
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

protected:
    explicit ClientSession(const std::string& host, const std::string& service);
    

protected:
    const std::string host;
    const std::string service;
    asio::ip::tcp::socket socket_;
    asio::ip::tcp::resolver resolver;
};
} // namespace proxy
} // namespace network