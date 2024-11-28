#pragma once
#include "fundamental/basic/buffer.hpp"
#include <cstdint>
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
    std::uint16_t fixed   = kFixed;
    std::uint8_t checkSum = 0;
    // version should be 0x01
    std::uint8_t version=kVersion;
    std::uint8_t op=0x00;
    union
    {
        std::uint8_t data[4];
        std::int32_t v;
    } mask;
    ProxySizeType sizeStorage = 0;
    Fundamental::Buffer<ProxySizeType> payload;
};
} // namespace proxy
} // namespace network