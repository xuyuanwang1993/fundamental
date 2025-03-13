
#pragma once


#include <string>
#include <vector>
#include "Connection.hpp"
#include "RequestHandler.hpp"

#include "fundamental/basic/utils.hpp"
namespace network::http
{

/// The top-level class of the HTTP server.
class Server
  : private Fundamental::NonCopyable
{
public:

    /// Construct the server to listen on the specified TCP address and port.
    explicit Server(  std::uint16_t port,
      const std::string& docRoot, std::size_t threadPoolSize);

    /// Run the server's io_service loop.
    void Run();

    // Set http handler
    using OnHttp = std::function<void(Reply&, Request&)>;
    void SetHttpHandler(OnHttp handler)
    {
        m_httpHandler = handler;
        if (m_newConnection)
            m_newConnection->SetHttpHandler(m_httpHandler);
    };

    void Stop();

private:
    /// Initiate an asynchronous accept operation.
    void StartAccept();

    /// Handle completion of an asynchronous accept operation.
    void HandleAccept(const ::asio::error_code& e);

    /// Handle a request to stop the server.
    void HandleStop();

    /// The number of threads that will call io_service::run().
    std::size_t m_threadPoolSize;

    /// The io_service used to perform asynchronous operations.
    ::asio::io_context m_ioService;

    /// The signal_set is used to register for process termination notifications.
    ::asio::signal_set m_signals;

    /// Acceptor used to listen for incoming connections.
    ::asio::ip::tcp::acceptor m_acceptor;

    /// The next connection to be accepted.
    ConnectionPtr m_newConnection;

    /// The handler for all incoming requests.
    RequestHandler m_requestHandler;

    // http handler
    OnHttp m_httpHandler;
};

} // namespace network::http