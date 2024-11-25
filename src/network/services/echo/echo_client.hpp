#pragma once
#include "echo_connection.hpp"
#include <atomic>
#include <future>
namespace network
{
namespace echo
{
using asio::ip::tcp;

typedef std::deque<EchoMsg> EchoMsg_queue;

class echo_client
{
public:
    echo_client(asio::io_context& io_context,
                const tcp::resolver::results_type& endpoints) :
    io_context_(io_context),
    socket_(io_context)
    {
        do_connect(endpoints);
    }

    void write(const EchoMsg& msg)
    {
        asio::post(io_context_,
                   [this, msg]() {
                       bool write_in_progress = !write_msgs_.empty();
                       write_msgs_.push_back(msg);
                       if (!write_in_progress)
                       {
                           do_write();
                       }
                   });
    }

    void close()
    {
        asio::post(io_context_, [this]() { socket_.close(); });
    }

    void wait_connect()
    {
        connect_promise_.get_future().wait();
    }

    bool connected()const
    {
        return is_connected.load();
    }
private:
    void do_connect(const tcp::resolver::results_type& endpoints)
    {
        asio::async_connect(socket_, endpoints,
                            [this](std::error_code ec, tcp::endpoint) {
                                if (!ec)
                                {
                                    is_connected.exchange(true);
                                    connect_promise_.set_value();
                                    do_read_header();
                                }
                                else
                                {
                                    FERR("connect failed {} {}",ec.message(),ec.value());
                                    connect_promise_.set_exception(std::make_exception_ptr(std::invalid_argument(ec.message())));
                                }
                            });
    }

    void do_read_header()
    {
        asio::async_read(socket_,
                         asio::buffer(msgContext_.msg.header.data, EchoMsg::kHeaderSize),
                         [this](std::error_code ec, std::size_t bytes_transferred) {
                             if (!ec)
                             {

                                 auto status = Paser::PaserRequest(msgContext_, bytes_transferred);
                                 switch (status)
                                 {
                                 case MsgContext::PaserNeedMoreData:
                                 {
                                     do_read_body();
                                 }
                                 break;
                                 default:
                                     handle_disconnected(ec);
                                     break;
                                 }
                             }
                             else
                             {
                                 handle_disconnected(ec);
                             }
                         });
    }

    void do_read_body()
    {
        asio::async_read(socket_,
                         asio::buffer(msgContext_.msg.msg.data(), msgContext_.msg.msg.size()),
                         [this](std::error_code ec, std::size_t bytes_transferred) {
                              if (!ec)
                             {

                                 auto status = Paser::PaserRequest(msgContext_, bytes_transferred);
                                 switch (status)
                                 {
                                 case MsgContext::PaserFinished:
                                 {
                                    msgContext_.msg.Dump();
                                    msgContext_.ClearStatus();
                                     do_read_header();
                                 }
                                 break;
                                 case MsgContext::PaserNeedMoreData:
                                 {
                                     do_read_body();
                                 }
                                 break;
                                 default:
                                     handle_disconnected(ec);
                                     break;
                                 }
                             }
                             else
                             {
                                 handle_disconnected(ec);
                             }
                         });
    }

    void do_write()
    {
        slice_data();
         socket_.async_write_some(
                          buf_slices_.front(),
                          [this](std::error_code ec, std::size_t length) {
                              if (!ec)
                              {
                                  peek_slice_front(length);
                                  if (!write_msgs_.empty())
                                  {
                                      do_write();
                                  }
                              }
                              else
                              {
                                  handle_disconnected(ec);
                              }
                          });
    }

    void slice_data()
    {
        if(buf_slices_.empty())
        {
            auto buffers=Builder::ToBuffers(write_msgs_.front());
            for(auto& item:buffers)
            {
                std::size_t offset=0;
                std::size_t leftSize=item.size();
                auto ptr=(const std::uint8_t *)item.data();
                while(leftSize>0)
                {
                    if(leftSize>connection::kPerReadMaxBytes)
                    {
                        buf_slices_.emplace_back(asio::const_buffer(ptr+offset,connection::kPerReadMaxBytes));
                        leftSize-=connection::kPerReadMaxBytes;
                        offset+=connection::kPerReadMaxBytes;
                    }
                    else
                    {
                        buf_slices_.emplace_back(asio::const_buffer(ptr+offset,leftSize));
                        leftSize-=leftSize;
                        offset+=leftSize;
                    }
                }
                
            }
        }
    }

    void peek_slice_front(std::size_t length)
    {
        auto& front=buf_slices_.front();
        if(length==front.size())
            buf_slices_.pop_front();
        else
        {
            front=asio::const_buffer((const std::uint8_t *)front.data()+length,front.size()-length);
        }
        if(buf_slices_.empty())
            write_msgs_.pop_front();
    }

    void handle_disconnected(std::error_code ec)
    {
        FWARN("disconnected {} {}",ec.message(),ec.value());
        socket_.close();
        is_connected.exchange(false);
    }
private:
    asio::io_context& io_context_;
    tcp::socket socket_;
    MsgContext msgContext_;
    EchoMsg_queue write_msgs_;
    std::deque<asio::const_buffer> buf_slices_;
    std::atomic<bool> is_connected=false;
    std::promise<void> connect_promise_;
};

} // namespace echo
} // namespace network