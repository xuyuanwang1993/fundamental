#pragma once

#include <asio.hpp>
#include <asio/detail/noncopyable.hpp>
#include <asio/steady_timer.hpp>

using tcp_socket = asio::ip::tcp::socket;

#include <string_view>
using string_view = std::string_view;
