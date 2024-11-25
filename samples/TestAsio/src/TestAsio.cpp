
#include "asio.hpp"
#include "fundamental/basic/log.h"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
using asio::ip::tcp;

class session
: public std::enable_shared_from_this<session>
{
public:
    session(tcp::socket socket) :
    socket_(std::move(socket))
    {
    }

    void start()
    {
        do_read();
    }

private:
    void do_read()
    {
        auto self(shared_from_this());
        socket_.async_read_some(asio::buffer(data_, max_length),
                                [this, self](std::error_code ec, std::size_t length) {
                                    if (!ec)
                                    {
                                        do_write(length);
                                    }
                                });
    }

    void do_write(std::size_t length)
    {
        auto self(shared_from_this());
        asio::async_write(socket_, asio::buffer(data_, length),
                          [this, self](std::error_code ec, std::size_t /*length*/) {
                              if (!ec)
                              {
                                  do_read();
                              }
                          });
    }

    tcp::socket socket_;
    enum
    {
        max_length = 1024
    };
    char data_[max_length];
};

class server
{
public:
    server(asio::io_context& io_context, short port) :
    acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        do_accept();
    }

private:
    void do_accept()
    {
        acceptor_.async_accept(
            [this](std::error_code ec, tcp::socket socket) {
                if (!ec)
                {
                    std::make_shared<session>(std::move(socket))->start();
                }

                do_accept();
            });
    }

    tcp::acceptor acceptor_;
};

enum
{
    max_length = 1024
};


int main(int argc, char* argv[])
{
    auto p = ::getenv("CLIENT");
    if (!p)
    {
        FINFO("you can set env \"CLIENT\" to perform as a client role");
        try
        {
            if (argc != 2)
            {
                FERR("Usage: async_tcp_echo_server <port>");
                return 1;
            }

            asio::io_context io_context;

            server s(io_context, std::atoi(argv[1]));

            io_context.run();
        }
        catch (std::exception& e)
        {
            FERR("Exception: {}",e.what());
        }

        return 0;
    }
    else
    {
        try
        {
            if (argc != 3)
            {
                FERR("Usage: blocking_tcp_echo_client <host> <port>");
                return 1;
            }

            asio::io_context io_context;

            tcp::socket s(io_context);
            tcp::resolver resolver(io_context);
            asio::connect(s, resolver.resolve(argv[1], argv[2]));

            std::cout << "Enter message: ";
            char request[max_length];
            std::cin.getline(request, max_length);
            size_t request_length = std::strlen(request);
            asio::write(s, asio::buffer(request, request_length));

            char reply[max_length];
            size_t reply_length = asio::read(s,
                                             asio::buffer(reply, request_length));
            FINFO("Reply is: {}",std::string(reply,reply_length));
        }
        catch (std::exception& e)
        {
            FERR("Exception: {}",e.what());
        }

        return 0;
    }
}
