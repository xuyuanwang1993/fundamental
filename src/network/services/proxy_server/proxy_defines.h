#pragma once

#include <cstdint>
#include <vector>

#include "fundamental/basic/buffer.hpp"
#include "fundamental/basic/log.h"
#include <asio.hpp>

namespace network
{
namespace proxy
{

enum ProxyOpCode : std::uint8_t
{
    AgentServiceOp = 0,
    TrafficProxyOp = 1
};

using ProxySizeType                     = std::uint64_t;
static constexpr std::size_t kProxySize = sizeof(ProxySizeType);
struct ProxyFrame
{
    static constexpr std::size_t kHeaderSize     = 2 /*fixed*/ + 1 /*checkSum*/ + 2 + 4 /*mask*/ + kProxySize /*payloadLen*/;
    static constexpr std::uint16_t kFixed        = 0x6668;
    static constexpr std::uint8_t kVersion       = 0x01;
    static constexpr ProxySizeType kMaxFrameSize = 512 * 1024; // 512k
    std::uint16_t fixed                          = kFixed;
    std::uint8_t checkSum                        = 0;
    // version should be 0x01
    std::uint8_t version = kVersion;
    std::uint8_t op      = 0x00;
    union
    {
        std::uint8_t data[4];
        std::int32_t v;
    } mask;
    ProxySizeType sizeStorage = 0;
    Fundamental::Buffer<ProxySizeType> payload;
    // functions

    std::vector<asio::const_buffer> ToAsioBuffers()
    {
        std::vector<asio::const_buffer> ret;
        ret.push_back(asio::const_buffer(&fixed, 2));
        ret.push_back(asio::const_buffer(&checkSum, 1));
        ret.push_back(asio::const_buffer(&version, 1));
        ret.push_back(asio::const_buffer(&op, 1));
        ret.push_back(asio::const_buffer(mask.data, 4));
        ret.push_back(asio::const_buffer(&sizeStorage, sizeof(sizeStorage)));
        ret.push_back(asio::const_buffer(payload.GetData(), payload.GetSize()));
        return ret;
    }
};

// a sample for payload

struct PayloadFrameHeader
{
    using SizeType                           = ProxySizeType;
    static constexpr std::size_t kSizeCount  = sizeof(SizeType);
    static constexpr std::size_t kHeaderSize = kSizeCount * 4;
    SizeType totalSize;
    SizeType cmdSize;
    SizeType descriptionSize;
    SizeType binarySize;
};

struct PayloadFrame
{
    using SizeType                             = PayloadFrameHeader::SizeType;
    using BinaryType                           = std::vector<std::uint8_t>;
    static constexpr std::size_t kMaxFrameSize = 1024LLU * 1024 * 1024 * 4; // 8G

    std::string cmd;
    std::string description;
    BinaryType binaryData;
    std::size_t size() const
    {
        return PayloadFrameHeader::kHeaderSize +
               cmd.size() +
               description.size() +
               binaryData.size();
    }
    static bool ReadFrameHeader(PayloadFrameHeader& header, std::uint8_t* data, std::size_t dataLen)
    {
        Fundamental::BufferReader<SizeType> reader;
        reader.SetBuffer(data, dataLen);
        try
        {
            reader.ReadValue(&header.totalSize);
            reader.ReadValue(&header.cmdSize);
            reader.ReadValue(&header.descriptionSize);
            reader.ReadValue(&header.binarySize);
            if (header.totalSize != (PayloadFrameHeader::kHeaderSize + header.cmdSize + header.descriptionSize + header.binarySize))
            {
                FDEBUG("payload size not matched");
                return false;
            }
            return true;
        }
        catch (const std::exception& e)
        {
            return false;
        }
    }

    static bool DecodeFrame(PayloadFrame& frame, const PayloadFrameHeader& header, std::uint8_t* data, std::size_t dataLen)
    {
        Fundamental::BufferReader<SizeType> reader;
        reader.SetBuffer(data, dataLen);
        try
        {
            frame.cmd.resize(header.cmdSize);
            frame.description.resize(header.descriptionSize);
            frame.binaryData.resize(header.binarySize);
            reader.ReadValue(frame.cmd.data(), header.cmdSize);
            reader.ReadValue(frame.description.data(), header.descriptionSize);
            reader.ReadValue(frame.binaryData.data(), header.binarySize);
            return true;
        }
        catch (const std::exception& e)
        {
            return false;
        }
    }
};

struct PayloadFrameEncodeView
{
    explicit PayloadFrameEncodeView(PayloadFrame& frame) :
    frame(frame)

    {
        header.totalSize       = htole64(frame.size());
        header.cmdSize         = htole64(static_cast<PayloadFrame::SizeType>(frame.cmd.size()));
        header.descriptionSize = htole64(static_cast<PayloadFrame::SizeType>(frame.description.size()));
        header.binarySize      = htole64(static_cast<PayloadFrame::SizeType>(frame.binaryData.size()));
    }

    std::vector<asio::const_buffer> ToAsioBuffers()
    {
        std::vector<asio::const_buffer> ret;
        ret.push_back(asio::const_buffer(&header.totalSize, PayloadFrameHeader::kSizeCount));
        ret.push_back(asio::const_buffer(&header.cmdSize, PayloadFrameHeader::kSizeCount));
        ret.push_back(asio::const_buffer(&header.descriptionSize, PayloadFrameHeader::kSizeCount));
        ret.push_back(asio::const_buffer(&header.binarySize, PayloadFrameHeader::kSizeCount));
        ret.push_back(asio::const_buffer(frame.cmd.data(), frame.cmd.size()));
        ret.push_back(asio::const_buffer(frame.description.data(), frame.description.size()));
        ret.push_back(asio::const_buffer(frame.binaryData.data(), frame.binaryData.size()));
        return ret;
    }

    const PayloadFrame& frame;
    PayloadFrameHeader header;
};

} // namespace proxy

namespace error
{
enum class proxy_errors : std::int32_t
{
    request_aborted      = -1,
    read_header_failed   = 0,
    read_payload_failed  = 1,
    parse_payload_failed = 2
};

class proxy_category : public std::error_category, public Fundamental::Singleton<proxy_category>
{
public:
    const char* name() const noexcept override
    {
        return "network.proxy";
    }
    std::string message(int value) const override
    {
        switch (static_cast<proxy_errors>(value))
        {
        case proxy_errors::request_aborted: return "request aborted";
        case proxy_errors::read_header_failed: return "read header failed";
        case proxy_errors::read_payload_failed: return "read payload failed";
        case proxy_errors::parse_payload_failed: return "parse payload failed";
        default: return "network.proxy error";
        }
    }
};

inline std::error_code make_error_code(proxy_errors e)
{
    return std::error_code(
        static_cast<int>(e), proxy_category::Instance());
}

} // namespace error

} // namespace network