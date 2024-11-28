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
        static_assert(4 == kProxySize || 8 == kProxySize, "unsupported proxy size type");
        if constexpr (4 == kProxySize)
        {
            payloadSize = le32toh(payloadSize);
        }
        else if constexpr (8 == kProxySize)
        {
            payloadSize = le64toh(payloadSize);
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
    opeationCheckSum.v                   = 0;
    auto bufferSize                      = dstFrame.payload.GetSize();
    auto ptr                             = dstFrame.payload.GetData();
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

std::vector<asio::const_buffer> ProxyRequestHandler::FrameToBuffers(const ProxyFrame& frame)
{
    std::vector<asio::const_buffer> ret;
    ret.push_back(asio::const_buffer(&frame.fixed, 8));
    ret.push_back(asio::const_buffer(&frame.payload.GetSize(), sizeof(decltype(frame.payload.GetSize()))));
    ret.push_back(asio::const_buffer(frame.payload.GetData(), frame.payload.GetSize()));
    return ret;
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
