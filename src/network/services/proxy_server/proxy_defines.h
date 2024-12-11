#pragma once
#include "fundamental/basic/buffer.hpp"
#include <asio.hpp>
#include <cstdint>
#include <vector>
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

struct PayloadFrame
{
    using SizeType                             = std::uint64_t;
    static constexpr std::size_t kMaxFrameSize = 1024LLU * 1024 * 1024 * 4; // 8G
    static constexpr std::size_t kSizeCount    = sizeof(SizeType);
    std::string cmd;
    std::string description;
    std::vector<std::uint8_t> binaryData;
    std::size_t size() const
    {
        return kSizeCount +
               sizeof(SizeType) +
               sizeof(SizeType) +
               sizeof(SizeType) +
               cmd.size() +
               description.size() +
               binaryData.size();
    }
    static bool ReadFrameSize(SizeType& size, std::uint8_t* data, std::size_t dataLen)
    {
        Fundamental::BufferReader<SizeType> reader;
        reader.SetBuffer(data, dataLen);
        try
        {
            reader.ReadValue(&size);
            return true;
        }
        catch (const std::exception& e)
        {
            return false;
        }
    }

    static bool DecodeFrame(PayloadFrame& frame, std::uint8_t* data, std::size_t dataLen)
    {
        Fundamental::BufferReader<SizeType> reader;
        reader.SetBuffer(data, dataLen);
        try
        {
            reader.ReadVectorLike(frame.cmd);
            reader.ReadVectorLike(frame.description);
            reader.ReadVectorLike(frame.binaryData);
            return true;
        }
        catch (const std::exception& e)
        {
            return false;
        }
    }
};

struct PayloadFrameView
{
    explicit PayloadFrameView(PayloadFrame& frame) :
    frame(frame),
    totalSize(htole64(frame.size())),
    cmdSize(htole64(static_cast<PayloadFrame::SizeType>(frame.cmd.size()))),
    descriptionSize(htole64(static_cast<PayloadFrame::SizeType>(frame.description.size()))),
    binarySize(htole64(static_cast<PayloadFrame::SizeType>(frame.binaryData.size())))
    {
    }

    std::vector<asio::const_buffer> ToAsioBuffers()
    {
        std::vector<asio::const_buffer> ret;
        ret.push_back(asio::const_buffer(&totalSize, PayloadFrame::kSizeCount));
        ret.push_back(asio::const_buffer(&cmdSize, PayloadFrame::kSizeCount));
        ret.push_back(asio::const_buffer(frame.cmd.data(), frame.cmd.size()));
        ret.push_back(asio::const_buffer(&descriptionSize, PayloadFrame::kSizeCount));
        ret.push_back(asio::const_buffer(frame.description.data(), frame.description.size()));
        ret.push_back(asio::const_buffer(&binarySize, PayloadFrame::kSizeCount));
        ret.push_back(asio::const_buffer(frame.binaryData.data(), frame.binaryData.size()));
        return ret;
    }

    const PayloadFrame& frame;
    const PayloadFrame::SizeType totalSize;
    const PayloadFrame::SizeType cmdSize;
    const PayloadFrame::SizeType descriptionSize;
    const PayloadFrame::SizeType binarySize;
};
} // namespace proxy
} // namespace network