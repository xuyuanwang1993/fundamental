#pragma once
#include "fundamental/basic/buffer.hpp"
#include <cstdint>
namespace network
{
namespace proxy
{

enum ProxyOpCode : std::uint8_t
{
    InformationOp  = 0,
    NetworkProxyOp = 1
};

using ProxySizeType                     = std::uint32_t;
static constexpr std::size_t kProxySize = sizeof(ProxySizeType);
struct ProxyFrame
{
    static constexpr std::size_t kHeaderSize     = 2 /*fixed*/ + 1 /*checkSum*/ + 1 /*version,op*/ + 4 /*mask*/ + kProxySize /*payloadLen*/;
    static constexpr std::uint16_t kFixed        = 0x6668;
    static constexpr std::uint8_t kVersion       = 0x01;
    static constexpr ProxySizeType kMaxFrameSize = 512 * 1024; // 512k
    ProxyFrame():version(kVersion),op(0x00){

    };
    std::uint16_t fixed                          = kFixed;
    std::uint8_t checkSum                        = 0;
    // version should be 0x01
    std::uint8_t version : 4;
    std::uint8_t op : 4;
    union
    {
        std::uint8_t data[4];
        std::int32_t v;
    } mask;
    Fundamental::Buffer<ProxySizeType> payload;
};
} // namespace proxy
} // namespace network