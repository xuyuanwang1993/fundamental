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

using ProxySizeType                     = std::uint32_t;
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
    //functions
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
} // namespace proxy
} // namespace network