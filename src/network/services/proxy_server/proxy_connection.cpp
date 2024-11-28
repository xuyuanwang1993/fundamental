#include "proxy_connection.hpp"
namespace network
{
namespace proxy
{

Connection::Connection(asio::ip::tcp::socket socket,
                       ProxyRequestHandler& handler) :
ConnectionInterface<ProxyRequestHandler>(std::move(socket), handler),
checkTimer(socket_.get_executor(), asio::chrono::seconds(kMaxRecvRequestFrameTimeSec))
{
}

void Connection::Start()
{
    StartTimerCheck();
    ReadHeader();
}

void Connection::HandleClose()
{
    StopTimeCheck();
    std::error_code ignored_ec;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both,
                     ignored_ec);
}

void Connection::ReadHeader()
{
    asio::async_read(socket_, asio::buffer(headerBuffer.data(), headerBuffer.size()),
                     [this, self = shared_from_this()](std::error_code ec, std::size_t bytes_transferred) {
                         do
                         {
                             if (ec)
                             {
                                 FWARN("disconnected for read header:[{}-{}] :{}", ec.category().name(), ec.value(), ec.message());
                                 break;
                             }
                             if (!request_handler_.DecodeHeader(headerBuffer.data(),
                                                                headerBuffer.size(), requestFrame))
                             {
                                 FWARN("decode header failed");
                                 break;
                             }
                             ReadBody();
                             return;
                         } while (0);
                         HandleClose();
                     });
}

void Connection::ReadBody()
{
    asio::async_read(socket_, asio::buffer(requestFrame.payload.GetData(), requestFrame.payload.GetSize()),
                     [this, self = shared_from_this()](std::error_code ec, std::size_t bytes_transferred) {
                         do
                         {
                             if (ec)
                             {
                                 FWARN("disconnected for read payload:[{}-{}] :{}", ec.category().name(), ec.value(), ec.message());
                                 break;
                             }
                             if (!request_handler_.DecodePayload(requestFrame))
                             {
                                 FWARN("decode payload failed");
                                 break;
                             }
                             StopTimeCheck();
                             request_handler_.UpgradeProtocal(std::forward<Connection>(*this));
                             return;
                         } while (0);
                         HandleClose();
                     });
}

void Connection::StartTimerCheck()
{
    checkTimer.async_wait([this, self = shared_from_this()](const std::error_code& e) {
        if (!e)
        {
            FWARN("recv request frame timeout,disconnected");
            HandleClose();
        }
    });
}

void Connection::StopTimeCheck()
{
    checkTimer.cancel();
}

} // namespace proxy
} // namespace network