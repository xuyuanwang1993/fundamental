#pragma once

#ifndef ASIO_USE_WOLFSSL
    #undef SHA256
#endif

#include <asio.hpp>
#include <asio/detail/noncopyable.hpp>
#include <asio/ssl.hpp>
#include <asio/steady_timer.hpp>
#include <string_view>

namespace network {
using tcp_socket  = asio::ip::tcp::socket;
using string_view = std::string_view;
} // namespace network
