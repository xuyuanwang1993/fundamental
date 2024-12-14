#include "proxy_request_handler.hpp"
#include "agent_service/agent_connection.hpp"
#include "fundamental/basic/log.h"
#include "fundamental/basic/utils.hpp"
#include "fundamental/delay_queue/delay_queue.h"
#include "proxy_connection.hpp"
#include "traffic_proxy_service/traffic_proxy_connection.hpp"

#include <map>
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
        std::uint32_t v;
    } opeationCheckSum;
    opeationCheckSum.v                   = 0;
    std::size_t leftSize                 = bufferSize % 4;
    decltype(bufferSize) alignBufferSize = bufferSize - leftSize;
    // restore to origin data
    for (decltype(bufferSize) i = 0; i < alignBufferSize; i += 4)
    {
        std::uint32_t& operationNum = *((std::uint32_t*)(ptr + i));
        operationNum ^= dstFrame.mask.v;
        opeationCheckSum.v ^= operationNum;
    }
    // update mask
    std::uint8_t checkSum=0;
    checkSum^= opeationCheckSum.data[0];
    checkSum ^= opeationCheckSum.data[1];
    checkSum ^= opeationCheckSum.data[2];
    checkSum ^= opeationCheckSum.data[3];
    // fix left bytes
    for (size_t i = 0; i < leftSize; ++i)
    {
        ptr[i + alignBufferSize] ^= dstFrame.mask.data[i % 4];
        checkSum^= ptr[i + alignBufferSize];
    }
    return dstFrame.checkSum == checkSum;
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

    auto op   = connection.requestFrame.op;
    auto iter = handlers_.find(op);
    if (iter != handlers_.end())
    {
        auto newConnection = iter->second(std::move(connection.socket_), std::move(connection.requestFrame));
        if (newConnection)
            newConnection->SetUp();
    }
    else
    {
        FWARN("unsupported proxy op {} for version {}", ProxyFrame::kVersion,
              connection.requestFrame.op);
    }
}

void ProxyRequestHandler::RegisterProtocal(std::uint8_t opCode, ProtocalHandler handler)
{
    auto iter = handlers_.find(opCode);
    if (iter != handlers_.end())
    {
        throw std::runtime_error(Fundamental::StringFormat("conflict proxy op:{}", opCode));
    }
    if (handler)
        handlers_[opCode] = handler;
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
