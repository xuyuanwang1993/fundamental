
#pragma once
#include <asio.hpp>
#include <array>
#include <memory>
#include "Reply.hpp"
#include "Request.hpp"
#include "RequestHandler.hpp"
#include "RequestParser.hpp"

#include "fundamental/basic/utils.hpp"

namespace network::http
{

/// Represents a single connection from a client.
class Connection
: public std::enable_shared_from_this<Connection>,
    private Fundamental::NonCopyable
{
public:
    /// Construct a connection with the given io_service.
    explicit Connection(::asio::io_context& ioService,
        RequestHandler& handler);

    /// Get the socket associated with the connection.
    ::asio::ip::tcp::socket& Socket();

    /// Start the first asynchronous operation for the connection.
    void Start();

    // Set http handler
    using OnHttp = std::function<void(Reply&, Request&)>;
    void SetHttpHandler(OnHttp handler)
    {
        m_httpHandler = handler;
    };

private:
    /// Handle completion of a read operation.
    void HandleRead(const ::asio::error_code& e,
        std::size_t bytesTransferred);

    /// Handle completion of a write operation.
    void HandleWrite(const ::asio::error_code& e);

    /// Strand to ensure the connection's handlers are not called concurrently.
    ::asio::io_context::strand m_strand;

    /// Socket for the connection.
    ::asio::ip::tcp::socket m_socket;

    /// The handler used to process the incoming request.
    RequestHandler& m_requestHandler;

    /// Buffer for incoming data.
    std::array<char, 8192> m_buffer;

    /// The incoming request.
    Request m_request;

    /// The parser for the incoming request.
    RequestParser m_requestParser;

    /// The reply to be sent back to the client.
    Reply m_reply;

    // http handler
    OnHttp m_httpHandler;
};

typedef std::shared_ptr<Connection> ConnectionPtr;

} // namespace network::http