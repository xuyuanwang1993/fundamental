#include "proxy_request_handler.hpp"
#include "fundamental/basic/log.h"
#include "fundamental/delay_queue/delay_queue.h"
#include "proxy_connection.hpp"
namespace network
{
namespace proxy
{
bool ProxyRequestHandler::DecodeHeader(const std::uint8_t* data, std::size_t len, ProxyFrame& dstFrame)
{
    do
    {
        if (len < ProxyFrame::kHeaderSize)
            break;
        std::size_t offset = 0;
        // docode fixed 2bytes
        std::memcpy(&dstFrame.fixed, data, 2);
        dstFrame.fixed = be16toh(dstFrame.fixed);
        if (dstFrame.fixed != ProxyFrame::kFixed)
            break;
        offset += 2;
        // decode checkSum 1bytes
        dstFrame.checkSum = data[offset];
        ++offset;
        // decode version low 4bit
        dstFrame.version = data[offset] & 0xf;
        if (dstFrame.version > ProxyFrame::kVersion)
            break;
        // decode op high 4bit
        dstFrame.op = data[offset] >> 4;
        ++offset;
        // decode mask 4bytes
        std::memcpy(dstFrame.mask.data, data + offset, 4);
        offset += 4;
        // decode payload size 4 bytes
        ProxySizeType payloadSize;
        std::memcpy(&payloadSize, data + offset, kProxySize);
        static_assert(4 == kProxySize || 8 == kProxySize , "unsupported proxy size type");
        if constexpr (4 == kProxySize)
        {
            payloadSize = be32toh(payloadSize);
        }
        else if constexpr (8 == kProxySize)
        {
            payloadSize = be64toh(payloadSize);
        }
        if (payloadSize > ProxyFrame::kMaxFrameSize)
            break;
        // allocate memory 4bytes align
        dstFrame.payload.Reallocate<sizeof(ProxySizeType)>(payloadSize);
        return true;
    } while (0);
    return false;
}

bool ProxyRequestHandler::DecodePayload(ProxyFrame& dstFrame)
{
    auto bufferSize = dstFrame.payload.GetSize();
    auto ptr        = dstFrame.payload.GetData();
    union
    {
        std::uint8_t data[4];
        std::int32_t v;
    } opeationCheckSum;
    opeationCheckSum.v = 0;
    // restore to origin data
    for (decltype(bufferSize) i = 0; i < bufferSize; i += 4)
    {
        std::int32_t& operationNum = *((std::int32_t*)ptr + i);
        operationNum ^= dstFrame.mask.v;
        opeationCheckSum.v ^= operationNum;
    }
    // update mask
    dstFrame.checkSum ^= opeationCheckSum.data[0];
    dstFrame.checkSum ^= opeationCheckSum.data[1];
    dstFrame.checkSum ^= opeationCheckSum.data[2];
    dstFrame.checkSum ^= opeationCheckSum.data[3];
    return dstFrame.checkSum == 0;
}

void ProxyRequestHandler::EncodeFrame(ProxyFrame& dstFrame)
{
    dstFrame.fixed   = htobe16(ProxyFrame::kFixed);
    dstFrame.version = ProxyFrame::kVersion;
    dstFrame.mask.v  = static_cast<std::int32_t>(Fundamental::Timer::GetTimeNow() & 0xffffffff);
    union
    {
        std::uint8_t data[4];
        std::int32_t v;
    } opeationCheckSum;
    opeationCheckSum.v = 0;
    auto bufferSize = dstFrame.payload.GetSize();
    auto ptr        = dstFrame.payload.GetData();
    // mask the origin data
    for (decltype(bufferSize) i = 0; i < bufferSize; i += 4)
    {
        std::int32_t& operationNum = *((std::int32_t*)ptr + i);
        operationNum ^= dstFrame.mask.v;
        opeationCheckSum.v ^= operationNum;
    }
    // update mask
    dstFrame.checkSum ^= opeationCheckSum.data[0];
    dstFrame.checkSum ^= opeationCheckSum.data[1];
    dstFrame.checkSum ^= opeationCheckSum.data[2];
    dstFrame.checkSum ^= opeationCheckSum.data[3];
}

void ProxyRequestHandler::UpgradeProtocal(Connection&& connection)
{
    //
}

} // namespace proxy
} // namespace network
