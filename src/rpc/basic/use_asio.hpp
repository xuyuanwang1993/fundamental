#pragma once

#include <asio.hpp>
#include <asio/detail/noncopyable.hpp>
#include <asio/steady_timer.hpp>
#include <string_view>

namespace network {
using tcp_socket = asio::ip::tcp::socket;
using string_view = std::string_view;
}

