#include "proxy_request_handler.hpp"
#include "agent_service/agent_connection.hpp"
#include "fundamental/basic/log.h"
#include "fundamental/delay_queue/delay_queue.h"
#include "proxy_connection.hpp"
#include "traffic_proxy_service/traffic_proxy_connection.hpp"
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
        dstFrame.fixed = le16toh(dstFrame.fixed);
        if (dstFrame.fixed != ProxyFrame::kFixed)
            break;
        offset += 2;
        // decode checkSum 1bytes
        dstFrame.checkSum = data[offset];
        ++offset;
        // decode version
        dstFrame.version = data[offset];
        if (dstFrame.version > ProxyFrame::kVersion)
            break;
        ++offset;
        // decode op
        dstFrame.op = data[offset];
        ++offset;
        // decode mask 4bytes
        std::memcpy(dstFrame.mask.data, data + offset, 4);
        offset += 4;
        // decode payload size 4 bytes
        std::memcpy(&dstFrame.sizeStorage, data + offset, kProxySize);
        static_assert(4 == kProxySize || 8 == kProxySize, "unsupported proxy size type");
        if constexpr (4 == kProxySize)
        {
            dstFrame.sizeStorage = le32toh(dstFrame.sizeStorage);
        }
        else if constexpr (8 == kProxySize)
        {
            dstFrame.sizeStorage = le64toh(dstFrame.sizeStorage);
        }
        if (dstFrame.sizeStorage > ProxyFrame::kMaxFrameSize)
            break;
        // allocate memory 4bytes align
        dstFrame.payload.Reallocate<sizeof(ProxySizeType)>(dstFrame.sizeStorage);
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
    opeationCheckSum.v                   = 0;
    std::size_t leftSize                 = bufferSize % 4;
    decltype(bufferSize) alignBufferSize = bufferSize - leftSize;
    // restore to origin data
    for (decltype(bufferSize) i = 0; i < alignBufferSize; i += 4)
    {
        std::int32_t& operationNum = *((std::int32_t*)(ptr + i));
        operationNum ^= dstFrame.mask.v;
        opeationCheckSum.v ^= operationNum;
    }

    // update mask
    dstFrame.checkSum ^= opeationCheckSum.data[0];
    dstFrame.checkSum ^= opeationCheckSum.data[1];
    dstFrame.checkSum ^= opeationCheckSum.data[2];
    dstFrame.checkSum ^= opeationCheckSum.data[3];
    // fix left bytes
    for (size_t i = 0; i < leftSize; ++i)
    {
        ptr[i + alignBufferSize] ^= dstFrame.mask.data[i % 4];
        dstFrame.checkSum ^= ptr[i + alignBufferSize];
    }
    return dstFrame.checkSum == 0;
}

void ProxyRequestHandler::EncodeFrame(ProxyFrame& dstFrame)
{
    dstFrame.fixed   = htole16(ProxyFrame::kFixed);
    dstFrame.version = ProxyFrame::kVersion;
    dstFrame.mask.v  = static_cast<std::int32_t>(Fundamental::Timer::GetTimeNow() & 0xffffffff);
    union
    {
        std::uint8_t data[4];
        std::int32_t v;
    } opeationCheckSum;
    opeationCheckSum.v = 0;
    auto bufferSize    = dstFrame.payload.GetSize();
    auto ptr           = dstFrame.payload.GetData();
    if constexpr (4 == kProxySize)
    {
        dstFrame.sizeStorage = le32toh(bufferSize);
    }
    else if constexpr (8 == kProxySize)
    {
        dstFrame.sizeStorage = le64toh(bufferSize);
    }
    std::size_t leftSize                 = bufferSize % 4;
    decltype(bufferSize) alignBufferSize = bufferSize - leftSize;
    // mask the origin data
    for (decltype(bufferSize) i = 0; i < alignBufferSize; i += 4)
    {
        std::int32_t& operationNum = *((std::int32_t*)(ptr + i));
        opeationCheckSum.v ^= operationNum;
        operationNum ^= dstFrame.mask.v;
    }
    // update mask
    dstFrame.checkSum ^= opeationCheckSum.data[0];
    dstFrame.checkSum ^= opeationCheckSum.data[1];
    dstFrame.checkSum ^= opeationCheckSum.data[2];
    dstFrame.checkSum ^= opeationCheckSum.data[3];
    // fix left bytes
    for (size_t i = 0; i < leftSize; ++i)
    {
        dstFrame.checkSum ^= ptr[i + alignBufferSize];
        ptr[i + alignBufferSize] ^= dstFrame.mask.data[i % 4];
    }
}

void ProxyRequestHandler::UpgradeProtocal(Connection&& connection)
{
    FDEBUG("proxy version {} op {} size:{}", connection.requestFrame.version,
           connection.requestFrame.op,
           connection.requestFrame.payload.GetSize());
    auto op = connection.requestFrame.op;
    switch (op)
    {
    case ProxyOpCode::AgentServiceOp:
    {
        std::make_shared<AgentConnection>(
            std::move(connection.socket_), std::move(connection.requestFrame))
            ->SetUp();
    }
    break;
    case ProxyOpCode::TrafficProxyOp:
    {
        std::make_shared<TrafficProxyConnection>(
            std::move(connection.socket_), std::move(connection.requestFrame))
            ->SetUp();
    }
    break;
    default:
    {
        FWARN("unsupported proxy op {} for version {}", ProxyFrame::kVersion,
              connection.requestFrame.op);
    }
    break;
    }
}

ProxeServiceBase::ProxeServiceBase(asio::ip::tcp::socket&& socket, ProxyFrame&& frame) :
socket_(std::forward<asio::ip::tcp::socket>(socket)),
frame(std::forward<ProxyFrame>(frame))
{
}

ProxeServiceBase::~ProxeServiceBase()
{
}

} // namespace proxy
} // namespace network
