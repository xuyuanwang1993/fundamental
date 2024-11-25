#pragma once
#include "network/server/basic_server.hpp"
#include <array>
#include <asio.hpp>
#include <deque>
#include <memory>
#include <vector>
namespace network
{
namespace echo
{

struct EchoMsg
{

    static constexpr std::size_t kHeaderSize    = sizeof(std::uint32_t);
    static constexpr std::size_t kTimeStampSize = 8;
    union
    {
        std::uint8_t data[kHeaderSize];
        std::uint32_t v;
    } header;
    std::vector<std::uint8_t> msg;
    std::int64_t& TimeStamp()
    {
        return *((std::int64_t*)msg.data());
    }
    std::uint8_t* Data()
    {
        return msg.data() + kTimeStampSize;
    }
    std::size_t Size()
    {
        return msg.size() - kTimeStampSize;
    }

    void Dump();
};

struct MsgContext
{

    enum
    {
        PaserFinished,
        PaserFailed,
        PaserWaitSize,
        PaserNeedMoreData,
        RequestExit,
    } status = PaserWaitSize;
    EchoMsg msg;
    std::int32_t currentOffset = 0;
    void ClearStatus();
    bool IsWaitSize();
};

struct EchoRequestHandler
{
    static void Process(const MsgContext& req, EchoMsg& reply);
    static void ProcessFailed(const std::string& msg, EchoMsg& reply);
};

struct Paser
{
    static constexpr std::size_t kMaxMsgSize = 32;
    static decltype(MsgContext::status) PaserRequest(MsgContext& msgContext, std::size_t dataLen);
};

struct Builder
{
    static std::vector<asio::const_buffer> ToBuffers(EchoMsg& reply);
};

/// Represents a single connection from a client.
class connection
: public std::enable_shared_from_this<connection>
{
public:
    static constexpr std::size_t kPerReadMaxBytes = 16;

public:
    connection(const connection&)            = delete;
    connection& operator=(const connection&) = delete;

    /// Construct a connection with the given socket.
    explicit connection(asio::ip::tcp::socket socket,
                        EchoRequestHandler& handler);

    /// Start the first asynchronous operation for the connection.
    void start();

private:
    void handle_close();
    /// Perform an asynchronous read operation.
    void do_read();

    /// Perform an asynchronous write operation.
    void do_write();

    /// Socket for the connection.
    asio::ip::tcp::socket socket_;

    /// The handler used to process the incoming msgContext.
    EchoRequestHandler& request_handler_;

    MsgContext msgContext_;
    std::deque<EchoMsg> replys_;
};

typedef std::shared_ptr<connection> connection_ptr;

using EchoServer=network::Server<connection,EchoRequestHandler>;
} // namespace echo
} // namespace network
