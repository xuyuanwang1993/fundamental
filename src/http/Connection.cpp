
#include "Connection.hpp"
#include <vector>
#include <optional>
#include <functional>
#include "RequestHandler.hpp"
#include "MimeTypes.hpp"

namespace network::http
{

Connection::Connection(::asio::io_context& ioService,
    RequestHandler& handler) :
m_strand(ioService),
m_socket(ioService),
m_requestHandler(handler)
{
}

::asio::ip::tcp::socket& Connection::Socket()
{
  return m_socket;
}

void Connection::Start()
{
    m_request.ip = m_socket.remote_endpoint().address().to_string();
    m_request.port = m_socket.remote_endpoint().port();
    m_socket.async_read_some(::asio::buffer(m_buffer),
        m_strand.wrap(
        std::bind(&Connection::HandleRead, shared_from_this(),
            std::placeholders::_1,//boost::asio::placeholders::error
            std::placeholders::_2)));//boost::asio::placeholders::bytes_transferred
}

void Connection::HandleRead(const ::asio::error_code& e,
                            std::size_t bytesTransferred)
{
    if (!e)
    {
        std::optional<bool> result;
        std::tie(result, std::ignore) = m_requestParser.Parse(
            m_request, m_buffer.data(), m_buffer.data() + bytesTransferred, bytesTransferred);

        auto resultHasVal = result.has_value();
        if (resultHasVal && result.value())
        {
            if (m_httpHandler)
            {
                m_httpHandler(m_reply, m_request);
            }
            else
            {
                m_reply = Reply::StockReply(Reply::not_found);
            }
            // m_requestHandler.HandleRequest(m_request, m_reply);

            ::asio::async_write(m_socket, m_reply.toBuffers<::asio::const_buffer>(),
                m_strand.wrap(
                std::bind(&Connection::HandleWrite, shared_from_this(),
                    std::placeholders::_1)));
        }
        else if (resultHasVal && !result.value())
        {
            m_reply = Reply::StockReply(Reply::bad_request);
            ::asio::async_write(m_socket, m_reply.toBuffers<::asio::const_buffer>(),
                m_strand.wrap(
                std::bind(&Connection::HandleWrite, shared_from_this(),
                    std::placeholders::_1)));
        }
        else
        {
            m_socket.async_read_some(::asio::buffer(m_buffer),
                m_strand.wrap(
                std::bind(&Connection::HandleRead, shared_from_this(),
                    std::placeholders::_1,
                    std::placeholders::_2)));
        }
    }

    // If an error occurs then no new asynchronous operations are started. This
    // means that all shared_ptr references to the connection object will
    // disappear and the object will be destroyed automatically after this
    // handler returns. The connection class's destructor closes the socket.
}

void Connection::HandleWrite(const ::asio::error_code& e)
{
    if (!e)
    {
        // Initiate graceful connection closure.
        ::asio::error_code ignored_ec;
        m_socket.shutdown(::asio::ip::tcp::socket::shutdown_both, ignored_ec);
    }

    // No new asynchronous operations are started. This means that all shared_ptr
    // references to the connection object will disappear and the object will be
    // destroyed automatically after this handler returns. The connection class's
    // destructor closes the socket.
}

} // namespace network::http