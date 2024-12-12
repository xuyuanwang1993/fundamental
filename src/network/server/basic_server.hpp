#pragma once
#include "fundamental/basic/log.h"
#include "fundamental/basic/utils.hpp"
#include "io_context_pool.hpp"
#include <asio.hpp>
#include <atomic>
#include <string>

namespace network
{
struct RequestHandlerDummy
{
};

// when data is coming we should parse the reuqest data first util a valid reuqest recv finished
// then we need a handler to process the request

template <typename RequestHandler>
struct ConnectionInterface : public Fundamental::NonCopyable
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
                    const std::string& port);

    /// start accept.
    /// exception on failure
    void Start();
    ///  stop accept
    void Stop();

protected:
    void DoAccept();

    void DoAwaitStop();
    std::atomic_bool has_started_ = false;

    /// Acceptor used to listen for incoming connections.
    std::shared_ptr<asio::ip::tcp::acceptor> acceptor_;

    /// The handler for all incoming requests.
    RequestHandler request_handler_;
    //
    std::string address;
    //
    std::string port;
};

template <typename Connection, typename RequestHandler>
inline Server<Connection, RequestHandler>::Server(const std::string& address,
                                                  const std::string& port) :
acceptor_(std::make_shared<asio::ip::tcp::acceptor>(io_context_pool::Instance().get_io_context())),
address(address),
port(port)
{
}

template <typename Connection, typename RequestHandler>
inline void Server<Connection, RequestHandler>::Start()
{
    bool expected_value = false;
    if (!has_started_.compare_exchange_strong(expected_value, true))
        return;
    // Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
    asio::ip::tcp::resolver resolver(acceptor_->get_executor());
    asio::ip::tcp::endpoint endpoint =
        *resolver.resolve(address, port).begin();
    acceptor_->open(endpoint.protocol());
    acceptor_->set_option(asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_->bind(endpoint);
    acceptor_->listen();

    DoAccept();
}

template <typename Connection, typename RequestHandler>
inline void Server<Connection, RequestHandler>::Stop()
{
    bool expected_value = true;
    if (!has_started_.compare_exchange_strong(expected_value, false))
        return;
    asio::post(io_context_pool::Instance().get_io_context(), [=, ref = std::weak_ptr<asio::ip::tcp::acceptor>(acceptor_)]() {
        auto instance = ref.lock();
        if (instance)
            instance->close();
    });
}

template <typename Connection, typename RequestHandler>
inline void Server<Connection, RequestHandler>::DoAccept()
{
    acceptor_->async_accept(io_context_pool::Instance().get_io_context(),
                            [this](std::error_code ec, asio::ip::tcp::socket socket) {
                                // Check whether the server was stopped by a signal before this
                                // completion handler had a chance to run.
                                if (!acceptor_->is_open())
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
}
template <typename RequestHandler>
inline ConnectionInterface<RequestHandler>::ConnectionInterface(asio::ip::tcp::socket socket, RequestHandler& handler_ref) :
socket_(std::move(socket)),
request_handler_(handler_ref)
{
}
} // namespace network