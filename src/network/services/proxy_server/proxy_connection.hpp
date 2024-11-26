#pragma once
#include "network/server/basic_server.hpp"
#include "proxy_request_handler.hpp"
#include "proxy_defines.h"
#include <asio.hpp>
#include <deque>
#include <memory>
#include <vector>
#include <array>
namespace network
{
namespace proxy
{

/// Represents a single Connection from a client.
class Connection
: public ConnectionInterface<ProxyRequestHandler>, public std::enable_shared_from_this<Connection>
{
    static constexpr std::size_t kMaxRecvRequestFrameTimeSec=30;
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
    std::array<std::uint8_t,ProxyFrame::kHeaderSize> headerBuffer;
    ProxyFrame requestFrame;
    asio::steady_timer checkTimer;
};

typedef std::shared_ptr<Connection> connection_ptr;

using EchoServer = network::Server<Connection, ProxyRequestHandler>;
} // namespace proxy
} // namespace network