//
// async_client.cpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2024 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//


#include <asio.hpp>
#include <functional>
#include <iostream>
#include <istream>
#include <ostream>
#include <string>

#include "fundamental/basic/log.h"
#include "rpc/proxy/proxy_encoder.h"

using asio::ip::tcp;

class client
{
public:
    client(asio::io_context& io_context,
           const std::string& server, const std::string& path) :
    resolver_(io_context),
    socket_(io_context)
    {
        // Form the request. We specify the "Connection: close" header so that the
        // server will close the socket after transmitting the response. This will
        // allow us to treat all data up until the EOF as the content.
        prepare_traffic(server);
        std::ostream request_stream(&request_);
        request_stream << "GET " << path << " HTTP/1.0\r\n";
        request_stream << "Host: " << server << "\r\n";
        request_stream << "Accept: */*\r\n";
        request_stream << "Connection: close\r\n\r\n";

        // Start an asynchronous resolve to translate the server and service names
        // into a list of endpoints.
        if (::getenv("USE_TRAFFIC_PROXY"))
        {
            resolver_.async_resolve("127.0.0.1", "9000",
                                    std::bind(&client::handle_resolve, this,
                                              asio::placeholders::error,
                                              asio::placeholders::results));
        }
        else
        {
            resolver_.async_resolve(server, "http",
                                    std::bind(&client::handle_resolve, this,
                                              asio::placeholders::error,
                                              asio::placeholders::results));
        }
        std::memset(&proxy_frame, 0, sizeof(proxy_frame));
    }
    ~client()
    {
        if (::getenv("USE_TRAFFIC_PROXY"))
        {
            proxy_free_output(&proxy_frame);
            FINFO("finish USE_TRAFFIC_PROXY");
        }
    }

private:
    void prepare_traffic(const std::string& host)
    {
        auto* ptr = ::getenv("USE_TRAFFIC_PROXY");
        if (!ptr)
            return;
        proxy_encode_input input;
        std::string proxyServiceName = "test_http";
        input.service                = proxyServiceName.data();
        input.serviceLen             = proxyServiceName.size();
        std::string field            = host;
        input.field                  = field.data();
        input.fieldLen               = field.size();
        std::string token            = "test_http_token";
        input.token                  = token.data();
        input.tokenLen               = token.size();
        proxy_encode_request(input, proxy_frame);
        buffers_.emplace_back(asio::const_buffer(proxy_frame.buf, proxy_frame.bufLen));
    }

    void handle_resolve(const std::error_code& err,
                        const tcp::resolver::results_type& endpoints)
    {
        if (!err)
        {
            // Attempt a connection to each endpoint in the list until we
            // successfully establish a connection.
            asio::async_connect(socket_, endpoints,
                                std::bind(&client::handle_connect, this,
                                          asio::placeholders::error));
        }
        else
        {
            std::cout << "Error: " << err.message() << "\n";
        }
    }

    void handle_traffic_proxy()
    {
        if (buffers_.empty())
            send_request();
        else
        {
            FINFO("write proxy handshake");
            asio::async_write(socket_, std::move(buffers_), [this](const std::error_code& err, std::size_t) {
                if (err)
                    return;
                handshake.resize(2);
                asio::async_read(socket_, asio::mutable_buffer(handshake.data(), 2), [this](const std::error_code& err, std::size_t) {
                    if (err || handshake != "ok")
                        return;
                    send_request();
                });
            });
        }
    }

    void send_request()
    {
        // The connection was successful. Send the request.
        asio::async_write(socket_, request_,
                          std::bind(&client::handle_write_request, this,
                                    asio::placeholders::error));
    }

    void handle_connect(const std::error_code& err)
    {
        if (!err)
        {
            handle_traffic_proxy();
        }
        else
        {
            std::cout << "Error: " << err.message() << "\n";
        }
    }

    void handle_write_request(const std::error_code& err)
    {
        if (!err)
        {
            // Read the response status line. The response_ streambuf will
            // automatically grow to accommodate the entire line. The growth may be
            // limited by passing a maximum size to the streambuf constructor.
            asio::async_read_until(socket_, response_, "\r\n",
                                   std::bind(&client::handle_read_status_line, this,
                                             asio::placeholders::error));
        }
        else
        {
            std::cout << "Error: " << err.message() << "\n";
        }
    }

    void handle_read_status_line(const std::error_code& err)
    {
        if (!err)
        {
            // Check that response is OK.
            std::istream response_stream(&response_);
            std::string http_version;
            response_stream >> http_version;
            unsigned int status_code;
            response_stream >> status_code;
            std::string status_message;
            std::getline(response_stream, status_message);
            if (!response_stream || http_version.substr(0, 5) != "HTTP/")
            {
                std::cout << "Invalid response\n";
                return;
            }
            if (status_code != 200)
            {
                std::cout << "Response returned with status code ";
                std::cout << status_code << "\n";
                return;
            }

            // Read the response headers, which are terminated by a blank line.
            asio::async_read_until(socket_, response_, "\r\n\r\n",
                                   std::bind(&client::handle_read_headers, this,
                                             asio::placeholders::error));
        }
        else
        {
            std::cout << "Error: " << err << "\n";
        }
    }

    void handle_read_headers(const std::error_code& err)
    {
        if (!err)
        {
            // Process the response headers.
            std::istream response_stream(&response_);
            std::string header;
            while (std::getline(response_stream, header) && header != "\r")
                std::cout << header << "\n";
            std::cout << "\n";

            // Write whatever content we already have to output.
            if (response_.size() > 0)
                std::cout << &response_;

            // Start reading remaining data until EOF.
            asio::async_read(socket_, response_,
                             asio::transfer_at_least(1),
                             std::bind(&client::handle_read_content, this,
                                       asio::placeholders::error));
        }
        else
        {
            std::cout << "Error: " << err << "\n";
        }
    }

    void handle_read_content(const std::error_code& err)
    {
        if (!err)
        {
            // Write all of the data that has been read so far.
            std::cout << &response_;

            // Continue reading remaining data until EOF.
            asio::async_read(socket_, response_,
                             asio::transfer_at_least(1),
                             std::bind(&client::handle_read_content, this,
                                       asio::placeholders::error));
        }
        else if (err != asio::error::eof)
        {
            std::cout << "Error: " << err << "\n";
        }
    }
    std::vector<asio::const_buffer> buffers_;
    proxy_encode_output proxy_frame;
    tcp::resolver resolver_;
    tcp::socket socket_;
    asio::streambuf request_;
    asio::streambuf response_;
    std::string handshake;
};

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 3)
        {
            std::cout << "Usage: client <server> <path>\n";
            std::cout << "Example:\n";
            std::cout << "  async_client www.baidu.com /\n";
            std::cout << "  set USE_TRAFFIC_PROXY=1 to test proxy\n";
            return 1;
        }

        asio::io_context io_context;
        client c(io_context, argv[1], argv[2]);
        io_context.run();
    }
    catch (std::exception& e)
    {
        std::cout << "Exception: " << e.what() << "\n";
    }

    return 0;
}
