#include "echo_connection.hpp"
#include "fundamental/basic/log.h"
#include "fundamental/delay_queue/delay_queue.h"
#include <utility>

namespace network
{
namespace echo
{

connection::connection(asio::ip::tcp::socket socket,
                       EchoRequestHandler& handler) :
ConnectionInterface<EchoRequestHandler>(std::move(socket),handler)
{
}

void connection::Start()
{
    do_read();
}

void connection::handle_close()
{
    // Initiate graceful connection closure.
    std::error_code ignored_ec;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both,
                     ignored_ec);
}

void connection::do_read()
{
    auto size = msgContext_.IsWaitSize() ? EchoMsg::kHeaderSize : msgContext_.msg.msg.size() - msgContext_.currentOffset;
    if (size > kPerReadMaxBytes)
        size = kPerReadMaxBytes;
    asio::async_read(socket_,
                     msgContext_.IsWaitSize() ? asio::buffer(&msgContext_.msg.header.data, size) : asio::buffer(msgContext_.msg.msg.data() + msgContext_.currentOffset, size),
                     [this, self = shared_from_this()](std::error_code ec, std::size_t bytes_transferred) {
                         if (!ec)
                         {

                             auto status = Paser::PaserRequest(msgContext_, bytes_transferred);
                             switch (status)
                             {
                             case MsgContext::PaserFinished:
                             {
                                 msgContext_.msg.Dump();
                                 replys_.emplace_back();
                                 request_handler_.Process(msgContext_, replys_.back());
                                 msgContext_.ClearStatus();
                                 // read next msgContext
                                 do_read();
                                 do_write();
                             }
                             break;
                             case MsgContext::PaserFailed:
                             {
                                 FWARN("failed msg");
                                 replys_.emplace_back();
                                 request_handler_.ProcessFailed("unsupported operation", replys_.back());
                                 msgContext_.ClearStatus();
                                 do_write();
                             }
                             break;
                             case MsgContext::RequestExit:
                             {
                                 FWARN("active msgContext disconnected");
                                 handle_close();
                             }

                             break;
                             default:
                                 do_read();
                                 break;
                             }
                         }
                         else
                         {
                             FWARN("disconnected for read :{}", ec.message());
                             handle_close();
                         }

                         // If an error occurs then no new asynchronous operations are
                         // started. This means that all shared_ptr references to the
                         // connection object will disappear and the object will be
                         // destroyed automatically after this handler returns. The
                         // connection class's destructor closes the socket.
                     });
}

void connection::do_write()
{
    asio::async_write(socket_, Builder::ToBuffers(replys_.front()),
                      [this, self = shared_from_this()](std::error_code ec, std::size_t) {
                          if (!ec)
                          {
                              replys_.pop_front();
                              if (!replys_.empty())
                                  do_write();
                          }
                          else
                          {
                              FWARN("disconnected for  write :{}", ec.message());
                              handle_close();
                          }
                          // No new asynchronous operations are started. This means that
                          // all shared_ptr references to the connection object will
                          // disappear and the object will be destroyed automatically after
                          // this handler returns. The connection class's destructor closes
                          // the socket.
                      });
}

void MsgContext::ClearStatus()
{
    currentOffset = 0;
    status        = PaserWaitSize;
    msg.header.v  = 0;
    msg.msg.clear();
}

bool MsgContext::IsWaitSize()
{
    return status == MsgContext::PaserWaitSize;
}

void EchoRequestHandler::Process(const MsgContext& req, EchoMsg& reply)
{
    reply             = std::move(req.msg);
    reply.TimeStamp() = htobe64((Fundamental::Timer::GetTimeNow<std::chrono::seconds,std::chrono::system_clock>()+1200));
}

void EchoRequestHandler::ProcessFailed(const std::string& msg, EchoMsg& reply)
{
    reply.header.v = htobe32(msg.size() + EchoMsg::kTimeStampSize);
    reply.msg.resize(msg.size() + EchoMsg::kTimeStampSize);
    std::memcpy(reply.Data(), msg.data(), msg.size());
    reply.TimeStamp() = htobe64((Fundamental::Timer::GetTimeNow<std::chrono::seconds,std::chrono::system_clock>()-1200));
}

decltype(MsgContext::status) Paser::PaserRequest(MsgContext& msgContext, std::size_t dataLen)
{
    if (0 == dataLen)
        return msgContext.status = MsgContext::PaserFailed;
    if (msgContext.IsWaitSize())
    {
        if (dataLen != EchoMsg::kHeaderSize)
            return msgContext.status = MsgContext::PaserFailed;
        auto size = be32toh(msgContext.msg.header.v);
        if (size == EchoMsg::kTimeStampSize)
            return msgContext.status = MsgContext::RequestExit;
        if (size > kMaxMsgSize || size < EchoMsg::kTimeStampSize)
        {
            FERR("overflow max size or below to min size");
            return msgContext.status = MsgContext::PaserFailed;
        }
        msgContext.currentOffset = 0;
        msgContext.msg.msg.resize(size);
        return msgContext.status = MsgContext::PaserNeedMoreData;
    }
    else
    {
        if (dataLen + msgContext.currentOffset > msgContext.msg.msg.size())
            return msgContext.status = MsgContext::PaserFailed;
        msgContext.currentOffset += dataLen;
        if (msgContext.currentOffset == msgContext.msg.msg.size())
            return msgContext.status = MsgContext::PaserFinished;
        return msgContext.status = MsgContext::PaserNeedMoreData;
    }
}

std::vector<asio::const_buffer> Builder::ToBuffers(EchoMsg& reply)
{
    std::vector<asio::const_buffer> ret;
    ret.push_back(asio::const_buffer(reply.header.data, EchoMsg::kHeaderSize));
    ret.push_back(asio::const_buffer(reply.msg.data(), reply.msg.size()));
    return ret;
}

void EchoMsg::Dump()
{
    std::int64_t tp = be64toh(TimeStamp());
    FINFO("{} {}", Fundamental::Timer::ToTimeStr(tp), std::string(Data(), Data() + Size()));
}

} // namespace echo
} // namespace network
