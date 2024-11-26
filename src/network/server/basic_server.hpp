#pragma once
#include "fundamental/basic/log.h"
#include "fundamental/basic/utils.hpp"
#include "io_context_pool.hpp"
#include <asio.hpp>
#include <string>
namespace network
{
struct RequestHandlerDummy
{
};

// when data is coming we should parse the reuqest data first util a valid reuqest recv finished
// then we need a handler to process the request

template <typename RequestHandler>
struct ConnectionInterface
{
    explicit ConnectionInterface(asio::ip::tcp::socket socket, RequestHandler& handler_ref);
    /// @brief called when a new connection set up
    virtual void Start() = 0;
    /// Socket for the connection.
    asio::ip::tcp::socket socket_;

    /// The handler used to process the incoming msgContext.
    RequestHandler& request_handler_;
};

// RequestHandler maybe provide hwo a request is handled
template <typename Connection, typename RequestHandler>
class Server : public Fundamental::NonCopyable
{
public:
    /// Construct the server to listen on the specified TCP address and port, and
    /// serve up files from the given directory.
    explicit Server(const std::string& address,
                    const std::string& port,
                    std::size_t io_context_pool_size = 2);

    /// Run the server's io_context loop.
    void Run();
    ///  request exit
    void RequestExit();

protected:
    void DoAccept();

    void DoAwaitStop();

    /// The pool of io_context objects used to perform asynchronous operations.
    io_context_pool io_context_pool_;

    /// The signal_set is used to register for process termination notifications.
    asio::signal_set signals_;

    /// Acceptor used to listen for incoming connections.
    asio::ip::tcp::acceptor acceptor_;

    /// The handler for all incoming requests.
    RequestHandler request_handler_;
};

template <typename Connection, typename RequestHandler>
inline Server<Connection, RequestHandler>::Server(const std::string& address,
                                                  const std::string& port,
                                                  std::size_t io_context_pool_size) :
io_context_pool_(io_context_pool_size),
signals_(io_context_pool_.get_io_context()),
acceptor_(io_context_pool_.get_io_context())
{
    // Register to handle the signals that indicate when the server should exit.
    // It is safe to register for the same signal multiple times in a program,
    // provided all registration for the specified signal is made through Asio.
    signals_.add(SIGINT);
    signals_.add(SIGTERM);
#if defined(SIGQUIT)
    signals_.add(SIGQUIT);
#endif // defined(SIGQUIT)

    DoAwaitStop();

    // Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
    asio::ip::tcp::resolver resolver(acceptor_.get_executor());
    asio::ip::tcp::endpoint endpoint =
        *resolver.resolve(address, port).begin();
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();

    DoAccept();
}

template <typename Connection, typename RequestHandler>
inline void Server<Connection, RequestHandler>::Run()
{
    io_context_pool_.run();
}

template <typename Connection, typename RequestHandler>
inline void Server<Connection, RequestHandler>::RequestExit()
{
    io_context_pool_.get_io_context().get_executor().post([=]() {
        io_context_pool_.stop();
    });
}

template <typename Connection, typename RequestHandler>
inline void Server<Connection, RequestHandler>::DoAccept()
{
    acceptor_.async_accept(io_context_pool_.get_io_context(),
                           [this](std::error_code ec, asio::ip::tcp::socket socket) {
                               // Check whether the server was stopped by a signal before this
                               // completion handler had a chance to run.
                               if (!acceptor_.is_open())
                               {
                                   return;
                               }

                               if (!ec)
                               {
                                   std::make_shared<Connection>(
                                       std::move(socket), request_handler_)
                                       ->Start();
                               }

                               DoAccept();
                           });
}

template <typename Connection, typename RequestHandler>
inline void Server<Connection, RequestHandler>::DoAwaitStop()
{
    signals_.async_wait(
        [this](std::error_code ec, int signo) {
            FDEBUG("quit server because of  signal:{} ec:{}", signo, ec.message());
            io_context_pool_.stop();
        });
}
template <typename RequestHandler>
inline ConnectionInterface<RequestHandler>::ConnectionInterface(asio::ip::tcp::socket socket, RequestHandler& handler_ref):
socket_(std::move(socket)),
request_handler_(handler_ref)
{

}
} // namespace network