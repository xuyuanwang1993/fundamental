#pragma once
#include "fundamental/basic/log.h"
#include "fundamental/basic/utils.hpp"
#include "io_context_pool.hpp"
#include <asio.hpp>
#include <atomic>
#include <string>

namespace network {
struct RequestHandlerDummy {};

// when data is coming we should parse the reuqest data first util a valid reuqest recv finished
// then we need a handler to process the request

template <typename RequestHandler, typename StreamSocketType = asio::ip::tcp>
struct ConnectionInterface : public Fundamental::NonCopyable {
    using ConnectionSocket   = typename StreamSocketType::socket;
    using ConnectionAcceptor = typename StreamSocketType::acceptor;
    using ConnectionEndpoint = typename StreamSocketType::endpoint;
    explicit ConnectionInterface(ConnectionSocket socket, RequestHandler& handler_ref) :
    socket_(std::move(socket)), request_handler_(handler_ref) {};
    /// @brief called when a new connection set up
    virtual void Start()           = 0;
    virtual ~ConnectionInterface() = default;
    /// Socket for the connection.
    ConnectionSocket socket_;

    /// The handler used to process the incoming msgContext.
    RequestHandler& request_handler_;
};

// RequestHandler maybe provide hwo a request is handled
template <typename Connection, typename RequestHandler>
class Server : public Fundamental::NonCopyable {
public:
    using ConnectionSocket   = typename Connection::ConnectionSocket;
    using ConnectionAcceptor = typename Connection::ConnectionAcceptor;
    using ConnectionEndpoint = typename Connection::ConnectionEndpoint;

public:
    /// Construct the server to listen on the specified TCP address and port, and
    /// serve up files from the given directory.
    explicit Server(const ConnectionEndpoint& endpoint) :
    acceptor_(io_context_pool::Instance().get_io_context()), endpoint_(endpoint) {};

    /// start accept.
    /// exception on failure
    void Start();
    ///  stop accept
    void Stop();

    RequestHandler& GetHandler() {
        return request_handler_;
    }

protected:
    void DoAccept();

    std::atomic_bool has_started_ = false;

    /// Acceptor used to listen for incoming connections.
    ConnectionAcceptor acceptor_;
    /// The handler for all incoming requests.
    RequestHandler request_handler_;
    ConnectionEndpoint endpoint_;
};

template <typename Connection, typename RequestHandler>
inline void Server<Connection, RequestHandler>::Start() {
    bool expected_value = false;
    if (!has_started_.compare_exchange_strong(expected_value, true)) return;
    acceptor_.open(endpoint_.protocol());
    if constexpr (std::is_same_v<ConnectionAcceptor, asio::ip::tcp::acceptor>) {
        acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    }
    acceptor_.bind(endpoint_);
    acceptor_.listen();

    DoAccept();
}

template <typename Connection, typename RequestHandler>
inline void Server<Connection, RequestHandler>::Stop() {
    bool expected_value = true;
    if (!has_started_.compare_exchange_strong(expected_value, false)) return;
    try {
        // close acceptor directly
        acceptor_.close();
    } catch (const std::exception& e) {
    }
}

template <typename Connection, typename RequestHandler>
inline void Server<Connection, RequestHandler>::DoAccept() {
    acceptor_.async_accept(io_context_pool::Instance().get_io_context(),
                           [this](std::error_code ec, asio::ip::tcp::socket socket) {
                               // Check whether the server was stopped by a signal before this
                               // completion handler had a chance to run.
                               if (!acceptor_.is_open()) {
                                   return;
                               }

                               if (!ec) {
                                   std::make_shared<Connection>(std::move(socket), request_handler_)->Start();
                               }

                               DoAccept();
                           });
}

} // namespace network